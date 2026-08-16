#!/usr/bin/env python3
"""AZ-SDD v4 baseline/change/archive deterministic operations."""

from __future__ import annotations

import hashlib
import json
import re
import shutil
from datetime import datetime
from pathlib import Path
from typing import Any


V4_METADATA = ".az-sdd-v4.json"
CHANGE_METADATA = ".az-sdd-change.json"
CHANGE_ID_PATTERN = re.compile(r"^[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+$")
DELTA_HEADING = re.compile(
    r"(?m)^##\s+(ADDED|MODIFIED|REMOVED):\s+(specs/[A-Za-z0-9._-]+\.spec\.md)\s*$"
)
CONTENT_START = "<!-- AZ-SDD:CONTENT:START -->"
CONTENT_END = "<!-- AZ-SDD:CONTENT:END -->"
REQUIRED_CHANGE_FILES = (
    "proposal.md",
    "spec.delta.md",
    "design.md",
    "tasks.md",
    "evidence.md",
    "acceptance.md",
    "feedback.md",
)
RISK_GATE_MAP = {
    "multi_module": {"design_required", "full_test_required"},
    "public_api": {"design_required", "full_test_required"},
    "data_migration": {"design_required", "full_test_required", "rollback_plan_required"},
    "new_dependency": {"design_required"},
    "security": {"design_required", "full_test_required", "manual_acceptance_required"},
    "hardware": {"design_required", "full_test_required", "manual_acceptance_required"},
    "deployment": {"design_required", "full_test_required", "rollback_plan_required"},
    "irreversible": {"rollback_plan_required", "manual_acceptance_required"},
    "insufficient_tests": {"full_test_required", "manual_acceptance_required"},
    "unknown_scope": {"scope_clarification_required", "design_required"},
    "user_experience": {"manual_acceptance_required"},
}
RISK_GATES = tuple(sorted({gate for gates in RISK_GATE_MAP.values() for gate in gates}))


class V4Error(Exception):
    """Business error carrying a stable code and recovery instruction."""

    def __init__(self, code: str, message: str, recovery: str) -> None:
        super().__init__(message)
        self.code = code
        self.recovery = recovery


def az_root(project: Path) -> Path:
    return project / "az-sdd"


def is_v4_project(project: Path) -> bool:
    root = az_root(project)
    return (root / V4_METADATA).is_file() and (root / "baseline" / "specs").is_dir()


def legacy_spec_files(project: Path) -> list[Path]:
    root = az_root(project) / "spec"
    return sorted(root.glob("*.spec.md")) if root.is_dir() else []


def baseline_spec_files(project: Path) -> list[Path]:
    root = az_root(project) / "baseline" / "specs"
    return sorted(root.glob("*.spec.md")) if root.is_dir() else []


def active_spec_files(project: Path) -> list[Path]:
    files = baseline_spec_files(project)
    return files if is_v4_project(project) else legacy_spec_files(project)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def ensure_gitkeep(path: Path) -> None:
    """让空的 v4 结构可以被 Git 保存；已有内容时不制造占位符。"""
    if path.is_dir() and not any(path.iterdir()):
        write_text(path / ".gitkeep", "")


def safe_target(root: Path, relative: str) -> Path:
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as exc:
        raise V4Error("ERR_INVALID_INPUT", f"delta 目标越出 baseline: {relative}", "仅使用 specs/<name>.spec.md") from exc
    return candidate


def feature_ids(paths: list[Path]) -> list[str]:
    values = []
    for path in paths:
        match = re.match(r"(\d{4})-", path.name)
        if match:
            values.append(match.group(1))
    return sorted(values)


def migration_plan(project: Path) -> dict[str, Any]:
    root = az_root(project)
    if not root.is_dir():
        raise V4Error("ERR_PROJECT_NOT_INITIALIZED", f"{root} 不存在", "先初始化 AZ-SDD 项目")
    sources = legacy_spec_files(project)
    if not sources:
        raise V4Error("ERR_ARTIFACT_NOT_FOUND", "没有可迁移的 v3 spec", "检查 az-sdd/spec/ 是否包含 *.spec.md")
    baseline = root / "baseline" / "specs"
    copies: list[dict[str, str]] = []
    conflicts: list[str] = []
    for source in sources:
        target = baseline / source.name
        state = "create"
        if target.exists():
            state = "unchanged" if sha256_file(source) == sha256_file(target) else "conflict"
        copies.append({"source": str(source), "target": str(target), "state": state})
        if state == "conflict":
            conflicts.append(str(target))
    if conflicts:
        raise V4Error(
            "ERR_BASELINE_CONFLICT",
            "baseline 已存在且与 v3 spec 不同: " + ", ".join(conflicts),
            "先人工合并冲突，或恢复一致 baseline 后重试",
        )
    return {
        "from": "v3",
        "to": "v4",
        "copies": copies,
        "legacy_feature_ids": feature_ids(sources),
        "directories": ["baseline/specs", "changes", "archive", "feedback"],
    }


def init_v4(project: Path, title: str, apply: bool) -> dict[str, Any]:
    root = az_root(project)
    if root.exists() and any(root.iterdir()):
        raise V4Error("ERR_ALREADY_INITIALIZED", f"{root} 已存在且非空", "使用现有项目；v3 项目请执行 migrate")
    files = ["PROJECT.md", "STATUS.md", "TRACK.md", V4_METADATA]
    directories = ["baseline/specs", "changes", "archive", "feedback"]
    if apply:
        for name in directories:
            (root / name).mkdir(parents=True, exist_ok=True)
        for name in directories:
            ensure_gitkeep(root / name)
        write_text(
            root / "PROJECT.md",
            f"# {title}\n\n## 目标\n\nTODO: 描述项目长期目标。\n\n"
            "## 治理原则\n\n所有行为变化通过 `changes/` 提议，验收后归档并合并到 `baseline/`。\n",
        )
        write_text(root / "STATUS.md", "# 项目状态\n\n当前无开放 change。\n")
        write_text(root / "TRACK.md", "# 关键事件\n")
        metadata = {
            "format": "az-sdd-v4",
            "version": 4,
            "created_at": datetime.now().astimezone().isoformat(timespec="seconds"),
            "legacy_files_preserved": False,
        }
        write_text(root / V4_METADATA, json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")
    return {
        "format": "v4",
        "project": str(project),
        "root": str(root),
        "files": files,
        "directories": directories,
        "apply": apply,
        "dry_run": not apply,
    }


def migrate_v3_to_v4(project: Path, apply: bool) -> dict[str, Any]:
    plan = migration_plan(project)
    root = az_root(project)
    if apply:
        for name in plan["directories"]:
            (root / name).mkdir(parents=True, exist_ok=True)
        for item in plan["copies"]:
            if item["state"] == "create":
                shutil.copy2(item["source"], item["target"])
        project_file = root / "PROJECT.md"
        if not project_file.exists():
            write_text(
                project_file,
                "# AZ-SDD Project\n\n"
                "## 目标\n\n继承 v3 `INIT.md` 中的项目背景；后续在此维护长期目标与治理原则。\n\n"
                "## 变更治理\n\n所有行为变化通过 `changes/` 提议，验证后归档并合并到 `baseline/`。\n",
            )
        metadata_path = root / V4_METADATA
        if not metadata_path.exists():
            metadata = {
                "format": "az-sdd-v4",
                "version": 4,
                "migrated_from": "v3",
                "migrated_at": datetime.now().astimezone().isoformat(timespec="seconds"),
                "legacy_files_preserved": True,
            }
            write_text(metadata_path, json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")
        for name in plan["directories"]:
            ensure_gitkeep(root / name)
    current = feature_ids(baseline_spec_files(project)) if apply else plan["legacy_feature_ids"]
    plan.update({"apply": apply, "dry_run": not apply, "v4_feature_ids": current})
    return plan


def validate_change_id(change_id: str) -> str:
    normalized = change_id.strip().upper()
    if not CHANGE_ID_PATTERN.fullmatch(normalized):
        raise V4Error("ERR_INVALID_CHANGE_ID", f"无效 change ID: {change_id}", "使用 CHG-0001 等大写字母、数字和连字符格式")
    return normalized


def normalize_operation(operation: str) -> str:
    normalized = operation.strip().upper()
    if normalized not in {"ADDED", "MODIFIED", "REMOVED"}:
        raise V4Error("ERR_INVALID_INPUT", f"不支持 delta 操作: {operation}", "使用 ADDED、MODIFIED 或 REMOVED")
    return normalized


def normalize_target(target: str) -> str:
    normalized = target.replace("\\", "/").lstrip("/")
    if not re.fullmatch(r"specs/[A-Za-z0-9._-]+\.spec\.md", normalized):
        raise V4Error("ERR_INVALID_INPUT", f"无效 baseline 目标: {target}", "使用 specs/<name>.spec.md")
    return normalized


def assess_risk(factors: list[str] | None) -> dict[str, Any]:
    normalized = sorted(set(factors or []))
    invalid = [factor for factor in normalized if factor not in RISK_GATE_MAP]
    if invalid:
        raise V4Error("ERR_INVALID_RISK_FACTOR", f"未知风险因子: {', '.join(invalid)}", "使用 flow.yaml risk_factors 中定义的名称")
    active = {gate for factor in normalized for gate in RISK_GATE_MAP[factor]}
    return {
        "factors": normalized,
        "gates": {gate: gate in active for gate in RISK_GATES},
        "path": "expanded" if active else "fast",
    }


def change_init(
    project: Path,
    change_id: str,
    title: str,
    target: str,
    operation: str,
    apply: bool,
    risk_factors: list[str] | None = None,
) -> dict[str, Any]:
    if not is_v4_project(project):
        raise V4Error("ERR_V4_NOT_INITIALIZED", "项目尚未启用 v4", "先执行 migrate --from v3 --to v4 --apply")
    cid = validate_change_id(change_id)
    op = normalize_operation(operation)
    relative = normalize_target(target)
    risk = assess_risk(risk_factors)
    root = az_root(project)
    baseline_target = safe_target(root / "baseline", relative)
    if op == "ADDED" and baseline_target.exists():
        raise V4Error("ERR_BASELINE_CONFLICT", f"ADDED 目标已存在: {relative}", "改用 MODIFIED 或选择新目标")
    if op in {"MODIFIED", "REMOVED"} and not baseline_target.is_file():
        raise V4Error("ERR_ARTIFACT_NOT_FOUND", f"{op} 目标不存在: {relative}", "检查 baseline 目标路径")
    change_dir = root / "changes" / cid
    if change_dir.exists():
        raise V4Error("ERR_CHANGE_EXISTS", f"change 已存在: {cid}", "使用其他 ID 或继续现有 change")
    base_hash = sha256_file(baseline_target) if baseline_target.exists() else "-"
    if op == "REMOVED":
        content = ""
    elif op == "MODIFIED":
        content = read_text(baseline_target).rstrip() + "\n"
    else:
        content = f"# {title}\n\nTODO: 描述新增的当前行为。\n"
    delta = f"# Spec Delta: {cid}\n\n## {op}: {relative}\n\nbase_sha256: {base_hash}\n"
    if op != "REMOVED":
        delta += f"\n{CONTENT_START}\n{content}{CONTENT_END}\n"
    risk_text = ", ".join(risk["factors"]) if risk["factors"] else "none（快速路径）"
    design_hint = "TODO: 记录风险触发的技术设计。" if risk["gates"].get("design_required") else "不需要独立设计。"
    files = {
        "proposal.md": f"# Proposal: {title}\n\n## Why\n\nTODO\n\n## Scope\n\nTODO\n\n## Non-goals\n\nTODO\n\n## Risk\n\n{risk_text}\n",
        "spec.delta.md": delta,
        "design.md": f"# Design: {title}\n\n{design_hint}\n",
        "tasks.md": f"# Tasks: {title}\n\n- [ ] 实现并验证 delta。\n- [ ] 更新 evidence 和 acceptance。\n",
        "evidence.md": f"# Evidence: {title}\n\nTODO: 记录测试命令、结果和实现证据。\n",
        "acceptance.md": f"# Acceptance: {title}\n\n验收结果: pending\n\nTODO: 记录人工验收。\n",
        "feedback.md": f"# Feedback: {title}\n\n无关联反馈时写“无”。\n",
    }
    manifest = {
        "change_id": cid,
        "title": title,
        "status": "proposed",
        "created_at": datetime.now().astimezone().isoformat(timespec="seconds"),
        "risk": risk,
        "operations": [{"operation": op, "target": relative, "base_sha256": base_hash}],
    }
    if apply:
        change_dir.mkdir(parents=True)
        for name, content_value in files.items():
            write_text(change_dir / name, content_value)
        write_text(change_dir / CHANGE_METADATA, json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    return {
        "change_id": cid,
        "path": str(change_dir),
        "operation": op,
        "target": relative,
        "base_sha256": base_hash,
        "risk": risk,
        "files": list(files) + [CHANGE_METADATA],
        "apply": apply,
        "dry_run": not apply,
    }


def change_directory(project: Path, change_id: str) -> Path:
    return az_root(project) / "changes" / validate_change_id(change_id)


def parse_delta(path: Path) -> list[dict[str, str]]:
    text = read_text(path)
    matches = list(DELTA_HEADING.finditer(text))
    if not matches:
        raise V4Error("ERR_DELTA_INVALID", f"{path} 没有合法 delta 块", "按 v4 delta 格式添加 ADDED/MODIFIED/REMOVED 块")
    operations: list[dict[str, str]] = []
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        block = text[match.end():end]
        hash_match = re.search(r"(?m)^base_sha256:\s*([0-9a-f]{64}|-)\s*$", block)
        if not hash_match:
            raise V4Error("ERR_DELTA_INVALID", f"{match.group(2)} 缺少合法 base_sha256", "保留 change-init 生成的哈希字段")
        operation = match.group(1)
        content = ""
        if operation != "REMOVED":
            start = block.find(CONTENT_START)
            finish = block.find(CONTENT_END)
            if start < 0 or finish < start:
                raise V4Error("ERR_DELTA_INVALID", f"{match.group(2)} 缺少内容标记", "补齐 AZ-SDD CONTENT START/END 标记")
            content = block[start + len(CONTENT_START):finish].strip("\r\n") + "\n"
        operations.append(
            {
                "operation": operation,
                "target": match.group(2),
                "base_sha256": hash_match.group(1),
                "content": content,
            }
        )
    targets = [item["target"] for item in operations]
    if len(targets) != len(set(targets)):
        raise V4Error("ERR_DELTA_INVALID", "同一 delta 中目标重复", "每个 baseline 文件只保留一个操作")
    return operations


def validate_change(project: Path, change_id: str, archive_gate: bool = True) -> dict[str, Any]:
    cid = validate_change_id(change_id)
    directory = change_directory(project, cid)
    issues: list[dict[str, str]] = []
    if not directory.is_dir():
        raise V4Error("ERR_CHANGE_NOT_FOUND", f"change 不存在: {cid}", "检查 change ID")
    for name in REQUIRED_CHANGE_FILES + (CHANGE_METADATA,):
        if not (directory / name).is_file():
            issues.append({"level": "critical", "code": "ERR_ARTIFACT_NOT_FOUND", "message": f"缺少 {name}"})
    operations: list[dict[str, str]] = []
    risk = assess_risk([])
    if not issues:
        try:
            manifest = json.loads(read_text(directory / CHANGE_METADATA))
            risk_data = manifest.get("risk", {})
            risk = assess_risk(risk_data.get("factors", []))
        except (json.JSONDecodeError, AttributeError, TypeError) as exc:
            raise V4Error("ERR_ARTIFACT_CONFLICT", f"change 元数据无法解析: {exc}", "修复 .az-sdd-change.json") from exc
        operations = parse_delta(directory / "spec.delta.md")
        baseline = az_root(project) / "baseline"
        for item in operations:
            target = safe_target(baseline, item["target"])
            op = item["operation"]
            if op == "ADDED" and target.exists():
                issues.append({"level": "critical", "code": "ERR_BASELINE_CONFLICT", "message": f"ADDED 目标已存在: {item['target']}"})
            if op in {"MODIFIED", "REMOVED"}:
                if not target.is_file():
                    issues.append({"level": "critical", "code": "ERR_ARTIFACT_NOT_FOUND", "message": f"目标不存在: {item['target']}"})
                elif sha256_file(target) != item["base_sha256"]:
                    issues.append({"level": "critical", "code": "ERR_BASELINE_CONFLICT", "message": f"baseline 已变化: {item['target']}"})
            if op != "REMOVED" and (not item["content"].strip() or "TODO:" in item["content"]):
                issues.append({"level": "critical", "code": "ERR_CHANGE_NOT_READY", "message": f"delta 内容未完成: {item['target']}"})
    if archive_gate:
        content_checks = {
            "proposal.md": ("TODO", "proposal 尚未完成"),
            "evidence.md": ("TODO", "evidence 尚未完成"),
            "acceptance.md": ("pending", "acceptance 尚未通过"),
        }
        for name, (marker, message) in content_checks.items():
            path = directory / name
            if path.is_file() and marker.lower() in read_text(path).lower():
                issues.append({"level": "critical", "code": "ERR_CHANGE_NOT_READY", "message": message})
        tasks = directory / "tasks.md"
        if tasks.is_file() and re.search(r"(?m)^\s*- \[ \]", read_text(tasks)):
            issues.append({"level": "critical", "code": "ERR_CHANGE_NOT_READY", "message": "仍有未完成任务"})
        acceptance = directory / "acceptance.md"
        if acceptance.is_file() and not re.search(r"(?im)验收结果\s*:\s*(?:passed|通过)", read_text(acceptance)):
            issues.append({"level": "critical", "code": "ERR_CHANGE_NOT_READY", "message": "缺少通过的验收结果"})
        gates = risk["gates"]
        design = directory / "design.md"
        if gates.get("design_required") and design.is_file():
            design_text = read_text(design)
            if "TODO" in design_text or "不需要独立设计" in design_text:
                issues.append({"level": "critical", "code": "ERR_RISK_GATE_REQUIRED", "message": "风险要求完整 design"})
        evidence = directory / "evidence.md"
        if gates.get("full_test_required") and evidence.is_file():
            if not re.search(r"(?im)(?:测试范围|test scope)\s*:\s*(?:full|完整)", read_text(evidence)):
                issues.append({"level": "critical", "code": "ERR_RISK_GATE_REQUIRED", "message": "风险要求完整测试范围"})
        if gates.get("manual_acceptance_required") and acceptance.is_file():
            if not re.search(r"(?im)(?:验收类型|acceptance type)\s*:\s*(?:manual|人工)", read_text(acceptance)):
                issues.append({"level": "critical", "code": "ERR_RISK_GATE_REQUIRED", "message": "风险要求人工验收"})
        if gates.get("rollback_plan_required"):
            combined = "\n".join(read_text(directory / name) for name in ("proposal.md", "design.md") if (directory / name).is_file())
            if not re.search(r"(?im)^##\s+(?:回滚|rollback)\s*$", combined):
                issues.append({"level": "critical", "code": "ERR_RISK_GATE_REQUIRED", "message": "风险要求回滚计划"})
        if gates.get("scope_clarification_required"):
            issues.append({"level": "critical", "code": "ERR_RISK_GATE_REQUIRED", "message": "影响范围未知，必须重新评估并移除 unknown_scope"})
    critical = sum(item["level"] == "critical" for item in issues)
    return {"change_id": cid, "issues": issues, "critical": critical, "operations": operations, "risk": risk, "ready": critical == 0}


def create_backup(project: Path, change_id: str) -> Path:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    backup = project / ".ai_tmp" / "bak" / "az-sdd-v4" / f"{stamp}-{change_id}"
    root = az_root(project)
    baseline = root / "baseline"
    change = root / "changes" / change_id
    if baseline.exists():
        shutil.copytree(baseline, backup / "az-sdd" / "baseline")
    if change.exists():
        shutil.copytree(change, backup / "az-sdd" / "changes" / change_id)
    return backup


def restore_backup(project: Path, backup: Path, change_id: str, archive: Path) -> None:
    root = az_root(project)
    baseline = root / "baseline"
    saved_baseline = backup / "az-sdd" / "baseline"
    if baseline.exists():
        shutil.rmtree(baseline)
    if saved_baseline.exists():
        shutil.copytree(saved_baseline, baseline)
    change = root / "changes" / change_id
    saved_change = backup / "az-sdd" / "changes" / change_id
    if change.exists():
        shutil.rmtree(change)
    if saved_change.exists():
        shutil.copytree(saved_change, change)
    if archive.exists():
        shutil.rmtree(archive)


def archive_change(project: Path, change_id: str, apply: bool) -> dict[str, Any]:
    validation = validate_change(project, change_id, archive_gate=True)
    if not validation["ready"]:
        messages = "; ".join(item["message"] for item in validation["issues"])
        conflict = any(item["code"] == "ERR_BASELINE_CONFLICT" for item in validation["issues"])
        risk_blocked = any(item["code"] == "ERR_RISK_GATE_REQUIRED" for item in validation["issues"])
        code = "ERR_BASELINE_CONFLICT" if conflict else ("ERR_RISK_GATE_REQUIRED" if risk_blocked else "ERR_CHANGE_NOT_READY")
        recovery = "基于最新 baseline 重放 delta 后重试" if conflict else ("完成风险触发的条件门禁后重试" if risk_blocked else "完成任务、证据和验收后重试")
        raise V4Error(code, messages, recovery)
    cid = validation["change_id"]
    root = az_root(project)
    source = root / "changes" / cid
    archive = root / "archive" / f"{datetime.now().date().isoformat()}-{cid}"
    if archive.exists():
        raise V4Error("ERR_ARCHIVE_EXISTS", f"归档目标已存在: {archive}", "检查是否已经归档或选择新的 change ID")
    if not apply:
        return {
            "change_id": cid,
            "operations": validation["operations"],
            "archive": str(archive),
            "apply": False,
            "dry_run": True,
            "backup": None,
        }
    backup = create_backup(project, cid)
    try:
        baseline = root / "baseline"
        for item in validation["operations"]:
            target = safe_target(baseline, item["target"])
            if item["operation"] == "REMOVED":
                target.unlink()
            else:
                write_text(target, item["content"])
        archive.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, archive)
        manifest_path = archive / CHANGE_METADATA
        manifest = json.loads(read_text(manifest_path))
        manifest.update({"status": "archived", "archived_at": datetime.now().astimezone().isoformat(timespec="seconds")})
        write_text(manifest_path, json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
        shutil.rmtree(source)
    except Exception as exc:
        restore_backup(project, backup, cid, archive)
        raise V4Error("ERR_ARCHIVE_FAILED", f"归档失败并已自动回滚: {exc}", f"检查备份 {backup}") from exc
    return {
        "change_id": cid,
        "operations": validation["operations"],
        "archive": str(archive),
        "apply": True,
        "dry_run": False,
        "backup": str(backup),
    }


def v4_status(project: Path) -> dict[str, Any]:
    root = az_root(project)
    changes = []
    changes_root = root / "changes"
    if changes_root.is_dir():
        for path in sorted(changes_root.iterdir()):
            if path.is_dir():
                risk = assess_risk([])
                metadata = path / CHANGE_METADATA
                if metadata.is_file():
                    try:
                        risk = assess_risk(json.loads(read_text(metadata)).get("risk", {}).get("factors", []))
                    except (json.JSONDecodeError, AttributeError, TypeError, V4Error):
                        pass
                changes.append({"change_id": path.name, "path": str(path), "risk": risk})
    archives = [path.name for path in sorted((root / "archive").iterdir()) if path.is_dir()] if (root / "archive").is_dir() else []
    return {
        "format": "v4" if is_v4_project(project) else "v3",
        "baseline_feature_ids": feature_ids(baseline_spec_files(project)),
        "open_changes": changes,
        "archives": archives,
    }
