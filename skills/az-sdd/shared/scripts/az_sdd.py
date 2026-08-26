#!/usr/bin/env python3
"""AZ-SDD v4 deterministic project validator and change helper.

The tool intentionally uses only Python's standard library. State names and
transitions are parsed from states.yaml so the YAML file remains the source of
truth. Writes are conservative: existing files are backed up and only marked
auto-generated sections are rebuilt.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

import v4_model


SKILL_ROOT = Path(__file__).resolve().parents[2]
RULE_ROOT = SKILL_ROOT / "az-sdd-rules" / "az-sdd-rules-config"
STATE_FILE = RULE_ROOT / "states.yaml"
NAMING_FILE = RULE_ROOT / "naming.yaml"

STATE_PATTERNS = (
    re.compile(r"(?mi)^status\s*:\s*([^\r\n#]+)"),
    re.compile(r"(?mi)^\|\s*(?:当前)?状态\s*\|\s*([^|]+)\|"),
    re.compile(r"(?mi)^\*\*(?:当前)?状态\*\*\s*[:：]\s*([^\r\n]+)"),
)


def now_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def make_result(
    skill: str,
    status: str = "success",
    data: dict[str, Any] | None = None,
    warnings: list[str] | None = None,
    error: dict[str, str] | None = None,
    next_action: dict[str, str] | None = None,
) -> dict[str, Any]:
    return {
        "status": status,
        "timestamp": now_iso(),
        "skill": skill,
        "data": data or {},
        "warnings": warnings or [],
        "error": error,
        "next_action": next_action,
    }


def error_result(code: str, message: str, recovery: str) -> dict[str, Any]:
    return make_result(
        "az-sdd-rules",
        status="error",
        error={"code": code, "message": message, "recovery": recovery},
    )


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def strip_comment(value: str) -> str:
    return value.split("#", 1)[0].strip().strip('"').strip("'")


class StateRules:
    """Parse the limited state-related YAML structures used by AZ-SDD."""

    def __init__(self, path: Path = STATE_FILE) -> None:
        self.path = path
        self.states: dict[str, str] = {}
        self.transitions: dict[str, list[str]] = {}
        self.feedback_states: dict[str, str] = {}
        self._parse()

    def _parse(self) -> None:
        if not self.path.exists():
            raise FileNotFoundError(self.path)
        section = ""
        current_state: str | None = None
        current_feedback: str | None = None
        for raw in read_text(self.path).splitlines():
            stripped = raw.strip()
            if not stripped or stripped.startswith("#"):
                continue
            indent = len(raw) - len(raw.lstrip(" "))
            if indent == 0 and stripped.endswith(":"):
                section = stripped[:-1]
                current_state = None
                current_feedback = None
                continue
            if section == "states":
                if indent == 2 and stripped.startswith("- id:"):
                    current_state = strip_comment(stripped.split(":", 1)[1])
                    self.states[current_state] = current_state
                elif indent == 4 and stripped.startswith("name:") and current_state:
                    self.states[current_state] = strip_comment(stripped.split(":", 1)[1])
            elif section == "transitions":
                if indent == 2 and stripped.endswith(":"):
                    current_state = stripped[:-1]
                    self.transitions[current_state] = []
                elif indent == 4 and stripped.startswith("- ") and current_state:
                    self.transitions[current_state].append(strip_comment(stripped[2:]))
            elif section == "feedback_states":
                if indent == 2 and stripped.startswith("- id:"):
                    current_feedback = strip_comment(stripped.split(":", 1)[1])
                    self.feedback_states[current_feedback] = current_feedback
                elif indent == 4 and stripped.startswith("name:") and current_feedback:
                    self.feedback_states[current_feedback] = strip_comment(stripped.split(":", 1)[1])
        if not self.states or not self.transitions:
            raise ValueError(f"无法从 {self.path} 解析状态或转换规则")

    @property
    def aliases(self) -> dict[str, str]:
        result: dict[str, str] = {}
        for state_id, name in self.states.items():
            result[state_id.lower()] = state_id
            result[name.strip()] = state_id
        return result

    def normalize(self, value: str | None) -> str | None:
        if not value:
            return None
        clean = re.sub(r"[✅📋👁️🔨🧪🎯🚀🚧]", "", value).strip()
        clean = clean.split()[0].strip("`*_[]()（）")
        return self.aliases.get(clean, self.aliases.get(clean.lower()))

    def can_transition(self, source: str, target: str) -> bool:
        return target in self.transitions.get(source, [])


def parse_project_structure(path: Path = NAMING_FILE) -> tuple[list[str], list[str]]:
    """Read required root files and declared directories from naming.yaml."""
    files: list[str] = []
    directories: list[str] = []
    area = ""
    pending: str | None = None
    pending_indent = 0
    for raw in read_text(path).splitlines():
        stripped = raw.strip()
        indent = len(raw) - len(raw.lstrip(" "))
        if indent == 2 and stripped in {"files:", "directories:"}:
            area = stripped[:-1]
            pending = None
            continue
        if indent == 4 and stripped.startswith("- name:") and area:
            pending = strip_comment(stripped.split(":", 1)[1])
            pending_indent = indent
            if area == "directories":
                directories.append(pending)
            continue
        if area == "files" and pending and indent == 6 and stripped == "required: true":
            files.append(pending)
        if indent <= pending_indent and not stripped.startswith("- name:"):
            pending = None
    return files, directories


def archive_root(project: Path) -> Path:
    return project / "az-sdd"


def spec_files(project: Path) -> list[Path]:
    return v4_model.active_spec_files(project)


def arch_files(project: Path) -> list[Path]:
    root = archive_root(project) / "arch"
    return sorted(root.glob("*.arch.md")) if root.exists() else []


def feedback_files(project: Path) -> list[Path]:
    root = archive_root(project) / "feedback"
    return sorted(root.glob("FB-*.feedback.md")) if root.exists() else []


def feature_id(path: Path) -> str:
    match = re.match(r"(\d{4})-", path.name)
    return match.group(1) if match else ""


def extract_state(text: str, rules: StateRules) -> str | None:
    for pattern in STATE_PATTERNS:
        match = pattern.search(text)
        if match:
            normalized = rules.normalize(match.group(1))
            if normalized:
                return normalized
    for state_id, name in rules.states.items():
        if re.search(rf"(?m)^\s*(?:[-*]\s*)?(?:状态|当前状态)\s*[:：|]\s*.*(?:{re.escape(state_id)}|{re.escape(name)})", text):
            return state_id
    return None


def replace_state(text: str, target: str, rules: StateRules) -> str | None:
    display = rules.states[target]
    for pattern in STATE_PATTERNS:
        match = pattern.search(text)
        if not match:
            continue
        value_start, value_end = match.span(1)
        old = text[value_start:value_end]
        prefix = " " if old.startswith(" ") else ""
        suffix = " " if old.endswith(" ") else ""
        return text[:value_start] + prefix + display + suffix + text[value_end:]
    return None


def issue(level: str, code: str, message: str, path: Path | None = None) -> dict[str, str]:
    item = {"level": level, "code": code, "message": message}
    if path:
        item["path"] = str(path)
    return item


def validate_project(project: Path, rules: StateRules) -> list[dict[str, str]]:
    issues: list[dict[str, str]] = []
    root = archive_root(project)
    if not root.is_dir():
        return [issue("critical", "ERR_PROJECT_NOT_INITIALIZED", "缺少 az-sdd/ 项目档案", root)]
    if v4_model.is_v4_project(project):
        for name in ("PROJECT.md", v4_model.V4_METADATA):
            path = root / name
            if not path.is_file():
                issues.append(issue("critical", "ERR_ARTIFACT_NOT_FOUND", f"缺少 v4 必需文件 {name}", path))
        for name in ("baseline/specs", "changes", "archive", "feedback"):
            path = root / name
            if not path.is_dir():
                issues.append(issue("critical", "ERR_ARTIFACT_NOT_FOUND", f"缺少 v4 制品目录 {name}/", path))
    else:
        required_files, directories = parse_project_structure()
        for name in required_files:
            path = root / name
            if not path.is_file():
                issues.append(issue("critical", "ERR_ARTIFACT_NOT_FOUND", f"缺少必需文件 {name}", path))
        for name in directories:
            path = root / name
            if not path.is_dir():
                issues.append(issue("warning", "ERR_ARTIFACT_NOT_FOUND", f"缺少制品目录 {name}/", path))
    seen: set[str] = set()
    for path in spec_files(project):
        fid = feature_id(path)
        if not fid:
            issues.append(issue("critical", "ERR_INVALID_INPUT", "spec 文件名不符合四位编号约定", path))
            continue
        if fid in seen:
            issues.append(issue("critical", "ERR_ARTIFACT_CONFLICT", f"功能编号 {fid} 重复", path))
        seen.add(fid)
        state = extract_state(read_text(path), rules)
        if not state:
            issues.append(issue("critical", "ERR_INVALID_STATE", f"无法识别功能 {fid} 的状态", path))
    return issues


def analyze_project(project: Path, rules: StateRules) -> list[dict[str, str]]:
    issues = validate_project(project, rules)
    arch_ids = {feature_id(path) for path in arch_files(project)}
    reports_root = archive_root(project) / "test"
    report_names = [str(path).lower() for path in reports_root.rglob("*")] if reports_root.exists() else []
    coding_root = archive_root(project) / "coding"
    coding_text = "\n".join(read_text(path) for path in coding_root.glob("*.md")) if coding_root.exists() else ""
    for path in spec_files(project):
        text = read_text(path)
        fid = feature_id(path)
        state = extract_state(text, rules)
        strict = bool(re.search(r"(?i)(?:mode|模式)\s*[:：|]\s*strict", text))
        complex_hint = bool(re.search(r"多模块|架构变更|硬件依赖|部署架构", text))
        if (strict or complex_hint) and fid not in arch_ids:
            issues.append(issue("critical", "ERR_ARCH_REQUIRED", f"功能 {fid} 需要独立 arch", path))
        acceptance_count = len(re.findall(r"\[(?:AC)?\d{3}\]", text))
        acceptance_count += len(re.findall(r"(?m)^#{3,4}\s+(?:\d+(?:\.\d+)*\s+)?MT\d{3}", text))
        if acceptance_count == 0:
            issues.append(issue("warning", "ERR_ARTIFACT_NOT_FOUND", f"功能 {fid} 未识别到验收标准或 MT 用例", path))
        # coding 计划仅用于开发中的可追溯性；完成态以测试/验收证据为准。
        if state in {"developing", "testing", "acceptance"} and fid not in coding_text:
            issues.append(issue("warning", "ERR_ARTIFACT_NOT_FOUND", f"功能 {fid} 缺少可追溯的 coding 计划", path))
        if state in {"completed", "released"} and not any(fid in name for name in report_names):
            issues.append(issue("critical", "ERR_ARTIFACT_NOT_FOUND", f"已完成的功能 {fid} 缺少测试/验收证据", path))
    for path in feedback_files(project):
        text = read_text(path)
        closed = re.search(r"已解决|已关闭", text)
        evidence = re.search(r"验证结果|关闭原因|测试命令|用户确认", text)
        if closed and not evidence:
            issues.append(issue("critical", "ERR_ARTIFACT_CONFLICT", "关闭反馈缺少验证证据或关闭原因", path))
    if v4_model.is_v4_project(project):
        for change in v4_model.v4_status(project)["open_changes"]:
            try:
                validation = v4_model.validate_change(project, change["change_id"], archive_gate=False)
                for item in validation["issues"]:
                    issues.append(issue(item["level"], item["code"], f"{change['change_id']}: {item['message']}", Path(change["path"])))
            except v4_model.V4Error as exc:
                issues.append(issue("critical", exc.code, f"{change['change_id']}: {exc}", Path(change["path"])))
    return issues


def backup_file(path: Path, project: Path) -> None:
    if not path.exists():
        return
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    destination = project / ".ai_tmp" / "bak" / "az-sdd" / stamp / path.relative_to(project)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, destination)


def write_text(path: Path, text: str, project: Path, dry_run: bool) -> None:
    if dry_run:
        return
    backup_file(path, project)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def replace_auto_section(original: str, key: str, body: str) -> str:
    start = f"<!-- AZ-SDD:AUTO:{key}:START -->"
    end = f"<!-- AZ-SDD:AUTO:{key}:END -->"
    block = f"{start}\n{body.rstrip()}\n{end}"
    pattern = re.compile(re.escape(start) + r".*?" + re.escape(end), re.DOTALL)
    if pattern.search(original):
        return pattern.sub(block, original)
    return original.rstrip() + "\n\n" + block + "\n"


def build_spec_rows(project: Path, rules: StateRules) -> list[str]:
    rows = ["| 编号 | 功能 | 状态 | 规范 |", "|---|---|---|---|"]
    for path in spec_files(project):
        fid = feature_id(path) or "????"
        name = path.stem.removesuffix(".spec").removeprefix(fid + "-")
        state = extract_state(read_text(path), rules)
        display = rules.states.get(state or "", "未知")
        rows.append(f"| {fid} | {name} | {display} | [查看](./spec/{path.name}) |")
    return rows


def rebuild_indexes(project: Path, rules: StateRules, dry_run: bool) -> dict[str, Any]:
    root = archive_root(project)
    changed: list[str] = []
    if v4_model.is_v4_project(project):
        model = v4_model.v4_status(project)
        status_lines = [
            "## v4 自动状态摘要",
            "",
            f"- baseline 功能数：{len(model['baseline_feature_ids'])}",
            f"- 开放 change 数：{len(model['open_changes'])}",
            f"- archive 数：{len(model['archives'])}",
        ]
        status_path = root / "STATUS.md"
        original = read_text(status_path) if status_path.exists() else "# 项目状态\n"
        updated = replace_auto_section(original, "V4_STATUS", "\n".join(status_lines))
        if updated != original:
            write_text(status_path, updated, project, dry_run)
            changed.append(str(status_path))
        return {"changed": changed, "dry_run": dry_run, "format": "v4"}
    spec_index = root / "SPEC.md"
    original = read_text(spec_index) if spec_index.exists() else "# 功能规范索引\n"
    updated = replace_auto_section(original, "SPEC", "\n".join(build_spec_rows(project, rules)))
    if updated != original:
        write_text(spec_index, updated, project, dry_run)
        changed.append(str(spec_index))
    counts: Counter[str] = Counter()
    for path in spec_files(project):
        counts[extract_state(read_text(path), rules) or "unknown"] += 1
    status_lines = ["## 自动状态摘要", "", "| 状态 | 数量 |", "|---|---:|"]
    for state_id, name in rules.states.items():
        status_lines.append(f"| {name} | {counts.get(state_id, 0)} |")
    status_path = root / "STATUS.md"
    original = read_text(status_path) if status_path.exists() else "# 项目状态\n"
    updated = replace_auto_section(original, "STATUS", "\n".join(status_lines))
    if updated != original:
        write_text(status_path, updated, project, dry_run)
        changed.append(str(status_path))
    return {"changed": changed, "dry_run": dry_run}


def locate_feature(project: Path, fid: str) -> Path:
    matches = [path for path in spec_files(project) if feature_id(path) == fid]
    if not matches:
        raise FileNotFoundError(f"找不到功能 {fid} 的 spec")
    if len(matches) > 1:
        raise RuntimeError(f"功能 {fid} 存在多个 spec")
    return matches[0]


def transition_feature(
    project: Path,
    rules: StateRules,
    fid: str,
    source: str,
    target: str,
    reason: str,
    dry_run: bool,
) -> dict[str, Any]:
    if v4_model.is_v4_project(project):
        raise v4_model.V4Error(
            "ERR_TRANSITION_NOT_ALLOWED",
            "v4 baseline 不允许直接状态转换",
            "在 change 中完成任务和验收，再通过 archive 更新 baseline",
        )
    path = locate_feature(project, fid)
    text = read_text(path)
    actual = extract_state(text, rules)
    source_id = rules.normalize(source)
    target_id = rules.normalize(target)
    if not source_id or not target_id:
        raise ValueError("源状态或目标状态不在 states.yaml 中")
    if actual != source_id:
        raise RuntimeError(f"功能 {fid} 实际状态为 {actual or '未知'}，不是 {source_id}")
    if not rules.can_transition(source_id, target_id):
        allowed = ", ".join(rules.transitions.get(source_id, [])) or "无"
        raise PermissionError(f"不允许 {source_id} -> {target_id}；允许后继：{allowed}")
    updated = replace_state(text, target_id, rules)
    if updated is None:
        raise ValueError(f"无法在 {path} 中定位可写状态字段")
    write_text(path, updated, project, dry_run)
    track = archive_root(project) / "TRACK.md"
    event = f"- {now_iso()} | {fid} | {source_id} → {target_id} | {reason}\n"
    track_text = read_text(track) if track.exists() else "# 关键事件\n\n"
    write_text(track, track_text.rstrip() + "\n" + event, project, dry_run)
    rebuild = rebuild_indexes(project, rules, dry_run)
    return {"feature_id": fid, "from": source_id, "to": target_id, "reason": reason, "dry_run": dry_run, "rebuild": rebuild}


def status_data(project: Path, rules: StateRules) -> dict[str, Any]:
    items = []
    counts: Counter[str] = Counter()
    for path in spec_files(project):
        state = extract_state(read_text(path), rules) or "unknown"
        counts[state] += 1
        items.append({"feature_id": feature_id(path), "state": state, "path": str(path)})
    return {
        "project": str(project),
        "counts": dict(counts),
        "features": items,
        "change_model": v4_model.v4_status(project),
    }


def next_action(project: Path, rules: StateRules, issues: Iterable[dict[str, str]]) -> dict[str, str]:
    critical = [item for item in issues if item["level"] == "critical"]
    if critical:
        return {"skill": "az-sdd-rules", "reason": "先解决 critical 一致性问题", "prompt": "/az-sdd-rules analyze"}
    if v4_model.is_v4_project(project):
        open_changes = v4_model.v4_status(project)["open_changes"]
        if open_changes:
            cid = open_changes[0]["change_id"]
            return {
                "skill": "az-sdd-next",
                "reason": f"存在未归档变更 {cid}",
                "prompt": f"python az_sdd.py change-validate {cid} --project .",
            }
    open_feedback = []
    for path in feedback_files(project):
        text = read_text(path)
        if not re.search(r"已解决|已关闭", text):
            open_feedback.append(path)
    if open_feedback:
        return {"skill": "az-sdd-feedback", "reason": "存在未闭环反馈", "prompt": "/az-sdd-feedback resume"}
    states = [extract_state(read_text(path), rules) for path in spec_files(project)]
    if not states:
        return {"skill": "az-sdd-spec", "reason": "尚无功能规范", "prompt": "/az-sdd-spec"}
    priority = [
        ("draft", "az-sdd-spec", "/az-sdd-spec review"),
        ("reviewing", "az-sdd-spec", "/az-sdd-spec review"),
        ("approved", "az-sdd-coder", "/az-sdd-coder"),
        ("developing", "az-sdd-coder", "/az-sdd-coder continue"),
        ("testing", "az-sdd-tester", "/az-sdd-tester"),
        ("acceptance", "az-sdd-tester", "/az-sdd-tester close"),
    ]
    for state, skill, prompt in priority:
        if state in states:
            return {"skill": skill, "reason": f"存在 {state} 状态功能", "prompt": prompt}
    return {"skill": "az-sdd-track", "reason": "当前功能已完成，更新或汇报状态", "prompt": "/az-sdd-track"}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="AZ-SDD v4 project and change validator")
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("validate", "status", "next", "analyze", "rebuild-indexes"):
        item = sub.add_parser(name)
        item.add_argument("project", nargs="?", default=".")
        item.add_argument("--dry-run", action="store_true")
    transition = sub.add_parser("transition")
    transition.add_argument("feature_id")
    transition.add_argument("source")
    transition.add_argument("target")
    transition.add_argument("--project", default=".")
    transition.add_argument("--reason", required=True)
    transition.add_argument("--dry-run", action="store_true")
    init = sub.add_parser("init")
    init.add_argument("--project", default=".")
    init.add_argument("--title", required=True)
    init_mode = init.add_mutually_exclusive_group()
    init_mode.add_argument("--apply", action="store_true")
    init_mode.add_argument("--dry-run", action="store_true")
    migrate = sub.add_parser("migrate")
    migrate.add_argument("--from", dest="source_version", choices=("v3",), required=True)
    migrate.add_argument("--to", dest="target_version", choices=("v4",), required=True)
    migrate.add_argument("--project", default=".")
    migrate_mode = migrate.add_mutually_exclusive_group()
    migrate_mode.add_argument("--apply", action="store_true")
    migrate_mode.add_argument("--dry-run", action="store_true")
    change_init = sub.add_parser("change-init")
    change_init.add_argument("change_id")
    change_init.add_argument("--project", default=".")
    change_init.add_argument("--title", required=True)
    change_init.add_argument("--target", required=True)
    change_init.add_argument("--operation", choices=("ADDED", "MODIFIED", "REMOVED"), required=True)
    change_init.add_argument("--risk-factor", action="append", choices=tuple(v4_model.RISK_GATE_MAP), default=[])
    change_mode = change_init.add_mutually_exclusive_group()
    change_mode.add_argument("--apply", action="store_true")
    change_mode.add_argument("--dry-run", action="store_true")
    change_validate = sub.add_parser("change-validate")
    change_validate.add_argument("change_id")
    change_validate.add_argument("--project", default=".")
    archive = sub.add_parser("archive")
    archive.add_argument("change_id")
    archive.add_argument("--project", default=".")
    archive_mode = archive.add_mutually_exclusive_group()
    archive_mode.add_argument("--apply", action="store_true")
    archive_mode.add_argument("--dry-run", action="store_true")
    return parser.parse_args(argv)


def run(args: argparse.Namespace) -> dict[str, Any]:
    try:
        rules = StateRules()
    except (OSError, ValueError) as exc:
        return error_result("ERR_RULE_CONFIG", str(exc), "修复 states.yaml 后重试")
    project = Path(getattr(args, "project", ".")).resolve()
    if args.command not in {"transition", "init"} and not archive_root(project).is_dir():
        return error_result("ERR_PROJECT_NOT_INITIALIZED", f"{archive_root(project)} 不存在", "先显式调用 az-sdd-init")
    try:
        if args.command == "validate":
            issues = validate_project(project, rules)
            critical = sum(item["level"] == "critical" for item in issues)
            status = "error" if critical else ("warning" if issues else "success")
            return make_result("az-sdd-rules", status, {"issues": issues, "critical": critical})
        if args.command == "analyze":
            issues = analyze_project(project, rules)
            critical = sum(item["level"] == "critical" for item in issues)
            status = "error" if critical else ("warning" if issues else "success")
            return make_result("az-sdd-rules", status, {"issues": issues, "critical": critical})
        if args.command == "status":
            return make_result("az-sdd-track", data=status_data(project, rules))
        if args.command == "next":
            issues = analyze_project(project, rules)
            return make_result("az-sdd-next", data=status_data(project, rules), next_action=next_action(project, rules, issues))
        if args.command == "rebuild-indexes":
            return make_result("az-sdd-track", data=rebuild_indexes(project, rules, args.dry_run))
        if args.command == "transition":
            data = transition_feature(project, rules, args.feature_id, args.source, args.target, args.reason, args.dry_run)
            return make_result("az-sdd-rules", data=data)
        if args.command == "init":
            data = v4_model.init_v4(project, args.title, apply=args.apply)
            return make_result("az-sdd-init", data=data)
        if args.command == "migrate":
            data = v4_model.migrate_v3_to_v4(project, apply=args.apply)
            return make_result("az-sdd-init", data=data)
        if args.command == "change-init":
            data = v4_model.change_init(
                project,
                args.change_id,
                args.title,
                args.target,
                args.operation,
                apply=args.apply,
                risk_factors=getattr(args, "risk_factor", []),
            )
            return make_result("az-sdd-spec", data=data)
        if args.command == "change-validate":
            data = v4_model.validate_change(project, args.change_id, archive_gate=True)
            status = "success" if data["ready"] else "error"
            error = None
            if not data["ready"]:
                conflict = any(item["code"] == "ERR_BASELINE_CONFLICT" for item in data["issues"])
                risk_blocked = any(item["code"] == "ERR_RISK_GATE_REQUIRED" for item in data["issues"])
                code = "ERR_BASELINE_CONFLICT" if conflict else ("ERR_RISK_GATE_REQUIRED" if risk_blocked else "ERR_CHANGE_NOT_READY")
                error = {"code": code, "message": "change 未通过归档门禁", "recovery": "解决 data.issues 后重试"}
            return make_result("az-sdd-rules", status=status, data=data, error=error)
        if args.command == "archive":
            data = v4_model.archive_change(project, args.change_id, apply=args.apply)
            return make_result("az-sdd-track", data=data)
    except v4_model.V4Error as exc:
        return error_result(exc.code, str(exc), exc.recovery)
    except FileNotFoundError as exc:
        return error_result("ERR_ARTIFACT_NOT_FOUND", str(exc), "检查项目路径、功能编号和必需档案")
    except PermissionError as exc:
        return error_result("ERR_TRANSITION_NOT_ALLOWED", str(exc), "选择 states.yaml 允许的后继状态")
    except RuntimeError as exc:
        return error_result("ERR_ARTIFACT_CONFLICT", str(exc), "先解决重复或状态冲突")
    except ValueError as exc:
        return error_result("ERR_INVALID_INPUT", str(exc), "修正输入或档案格式后重试")
    except OSError as exc:
        return error_result("ERR_WRITE_PERMISSION", str(exc), "检查路径和写入权限，或先使用 --dry-run")
    return error_result("ERR_INVALID_INPUT", "未知命令", "使用 --help 查看支持的命令")


def main(argv: list[str] | None = None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    result = run(parse_args(argv))
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 1 if result["status"] == "error" else 0


if __name__ == "__main__":
    sys.exit(main())
