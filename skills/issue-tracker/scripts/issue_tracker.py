#!/usr/bin/env python3
"""Create and maintain issue-tracker Markdown records from fixed templates."""

from __future__ import annotations

import argparse
import json
import re
import shutil
from datetime import datetime
from pathlib import Path


SKILL_DIR = Path(__file__).resolve().parents[1]
TEMPLATE_DIR = SKILL_DIR / "templates"
DEFAULT_ROOT = Path(r"C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\奔霄-note\问题排查记录")
DEFAULT_ARCHIVE_ROOT = Path(r"C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\项目集")
INVALID_CHARS = r'<>:"/\|?*'
PLACEHOLDER = "待补充。"
ISSUE_DIR_SEPARATOR = "·"
DETAIL_FILENAME = "01_问题详情.md"
LOG_FILENAME = "02_排查记录.md"
REPORT_FILENAME = "03_总结汇报.md"
LEGACY_LOG_DIRNAME = "03_排查记录"
LEGACY_REPORT_FILENAME = "04_总结汇报.md"


class IssueTrackerError(Exception):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code
        self.message = message


def emit(status: str, **payload: object) -> None:
    print(json.dumps({"status": status, **payload}, ensure_ascii=False, indent=2))


def fail(error: IssueTrackerError) -> None:
    emit("error", error={"code": error.code, "message": error.message})
    raise SystemExit(1)


def sanitize_path_part(value: str, fallback: str) -> str:
    value = (value or "").strip()
    if not value:
        value = fallback
    value = re.sub(f"[{re.escape(INVALID_CHARS)}]", "-", value)
    value = re.sub(r"\s+", " ", value)
    value = value.strip(" .")
    return value or fallback


def read_template(name: str) -> str:
    path = TEMPLATE_DIR / name
    if not path.exists():
        raise IssueTrackerError("ERR_TEMPLATE_NOT_FOUND", f"模板不存在：{path}")
    return path.read_text(encoding="utf-8")


def render_template(name: str, data: dict[str, str]) -> str:
    text = read_template(name)
    for key, value in data.items():
        text = text.replace("{{" + key + "}}", value)
    return re.sub(r"{{[^{}]+}}", PLACEHOLDER, text)


def write_new(path: Path, content: str) -> None:
    if path.exists():
        raise IssueTrackerError("ERR_DUPLICATE_ISSUE", f"文件已存在，拒绝覆盖：{path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def write_replace(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def issue_dirs(root: Path) -> list[Path]:
    if not root.exists():
        return []
    return [p for p in root.iterdir() if p.is_dir()]


def find_issue(root: Path, query: str | None, issue_path: str | None = None) -> Path:
    if issue_path:
        path = Path(issue_path)
        if not path.exists() or not path.is_dir():
            raise IssueTrackerError("ERR_ISSUE_NOT_FOUND", f"未找到问题目录：{path}")
        return path

    if not query:
        raise IssueTrackerError("ERR_MISSING_REQUIRED_INFO", "缺少问题名称或 issue_path。")

    normalized = query.lower().strip()
    matches = [p for p in issue_dirs(root) if normalized in p.name.lower()]
    if not matches:
        recent = [p.name for p in list_recent(root)]
        raise IssueTrackerError("ERR_ISSUE_NOT_FOUND", "未找到匹配问题记录。最近记录：" + "；".join(recent))
    if len(matches) > 1:
        names = "；".join(p.name for p in matches[:10])
        raise IssueTrackerError("ERR_AMBIGUOUS_ISSUE", "匹配到多个问题记录：" + names)
    return matches[0]


def list_recent(root: Path, limit: int = 10) -> list[Path]:
    dirs = issue_dirs(root)
    return sorted(dirs, key=lambda p: p.stat().st_mtime, reverse=True)[:limit]


def require_fields(args: argparse.Namespace, fields: list[str]) -> None:
    missing = [field for field in fields if not getattr(args, field)]
    if missing:
        raise IssueTrackerError("ERR_MISSING_REQUIRED_INFO", "缺少字段：" + "、".join(missing))


def create_issue(args: argparse.Namespace) -> None:
    require_fields(args, ["title", "project", "reporter", "symptom"])
    root = Path(args.root)
    created_at = datetime.now()
    stamp = created_at.strftime("%Y-%m-%d")
    title = sanitize_path_part(args.title, "未命名问题")
    project = sanitize_path_part(args.project, "未命名项目")
    issue_dir = root / ISSUE_DIR_SEPARATOR.join([stamp, project, title])
    if issue_dir.exists():
        raise IssueTrackerError("ERR_DUPLICATE_ISSUE", f"问题目录已存在：{issue_dir}")

    data = {
        "issue_title": args.title,
        "project_name": args.project,
        "reporter": args.reporter,
        "priority": args.priority,
        "status": "待排查",
        "created_at": created_at.strftime("%Y-%m-%d %H:%M"),
        "symptom": args.symptom,
        "initial_judgement": args.initial_judgement or PLACEHOLDER,
        "impact_scope": args.impact_scope or PLACEHOLDER,
        "resolution_goal": args.resolution_goal or PLACEHOLDER,
        "issue_directory": str(issue_dir),
    }

    write_new(issue_dir / DETAIL_FILENAME, render_template("issue_detail.md", data))
    write_new(issue_dir / LOG_FILENAME, render_template("daily_log_placeholder.md", data))
    write_new(issue_dir / REPORT_FILENAME, render_template("report_placeholder.md", data))
    notification = render_template("notification.txt", data)

    emit(
        "success",
        action="create_issue",
        issue_directory=str(issue_dir),
        files=[
            str(issue_dir / DETAIL_FILENAME),
            str(issue_dir / LOG_FILENAME),
            str(issue_dir / REPORT_FILENAME),
        ],
        notification=notification,
    )


def count_phases(markdown: str) -> int:
    """Scan a daily log file and return the highest existing phase number (0 if none)."""
    return max([int(m) for m in re.findall(r"##\s*(?:\d{4}-\d{2}-\d{2}\s*)?阶段\s*(\d+)", markdown)] + [0])


def build_phase_section(data: dict[str, str]) -> str:
    """Render a single phase block from the daily_log.md template (excludes file header)."""
    template = read_template("daily_log.md")
    body = template.split("---\n", 1)[1] if "---\n" in template else template
    for key, value in data.items():
        body = body.replace("{{" + key + "}}", value)
    return re.sub(r"{{[^{}]+}}", PLACEHOLDER, body).rstrip() + "\n"


def add_daily_log(args: argparse.Namespace) -> None:
    root = Path(args.root)
    issue_dir = find_issue(root, args.query, args.issue_path)
    date = args.date or datetime.now().strftime("%Y-%m-%d")
    now = datetime.now().strftime("%H:%M")
    log_path = issue_dir / LOG_FILENAME

    existing = read_if_exists(log_path)
    has_phases = count_phases(existing) > 0
    if has_phases and not args.append:
        emit(
            "success",
            action="add_daily_log",
            issue_directory=str(issue_dir),
            daily_log_file=str(log_path),
            existed=True,
            hint="排查记录已存在阶段，需要追加新阶段请加 --append",
        )
        return

    if has_phases and args.append:
        phase = count_phases(existing) + 1
    else:
        phase = 1

    data = {
        "date": date,
        "phase": str(phase),
        "phase_timestamp": now,
        "investigation_goal": args.investigation_goal or PLACEHOLDER,
        "analysis_process": args.analysis_process or PLACEHOLDER,
        "possible_cause": args.possible_cause or PLACEHOLDER,
        "verification_method": args.verification_method or PLACEHOLDER,
        "verification_result": args.verification_result or PLACEHOLDER,
        "conclusion": args.conclusion or PLACEHOLDER,
        "next_plan": args.next_plan or PLACEHOLDER,
    }
    section = build_phase_section(data)

    if existing.strip():
        # 追加模式：在文件末尾追加分隔线 + 新阶段小节
        base = existing.rstrip()
        if not has_phases:
            base = re.sub(r"\n*暂无排查记录。\s*$", "", base).rstrip()
        separator = "\n\n---\n\n" if not base.endswith("\n\n") else "\n---\n\n"
        write_replace(log_path, base + separator + section)
    else:
        # 首次创建：写入文件头（标题+说明）+ 第一个阶段
        header = "# 排查记录\n\n> 同一文件内可按\"阶段 1/2/3...\"追加多次排查；每次追加会自动写入当前时间戳并生成新阶段小节。\n> 文件已有阶段时默认拒绝覆盖，需要追加新阶段请加 `--append`。\n\n---\n\n"
        write_new(log_path, header + section)

    emit(
        "success",
        action="add_daily_log",
        issue_directory=str(issue_dir),
        daily_log_file=str(log_path),
        existed=False,
        phase=phase,
        phase_timestamp=now,
    )


def read_if_exists(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def extract_table_value(markdown: str, field: str) -> str:
    pattern = re.compile(rf"^\|\s*{re.escape(field)}\s*\|\s*(.*?)\s*\|$", re.MULTILINE)
    match = pattern.search(markdown)
    if not match:
        return ""
    value = match.group(1).strip()
    return "" if value == PLACEHOLDER else value


def generate_report(args: argparse.Namespace) -> None:
    root = Path(args.root)
    issue_dir = find_issue(root, args.query, args.issue_path)
    detail = read_if_exists(issue_dir / DETAIL_FILENAME)
    expected = read_if_exists(issue_dir / "02_期望结果.md")
    new_log = issue_dir / LOG_FILENAME
    legacy_logs = sorted((issue_dir / LEGACY_LOG_DIRNAME).glob("*.md")) if (issue_dir / LEGACY_LOG_DIRNAME).exists() else []
    log_sources = [new_log] if new_log.exists() else legacy_logs
    log_text = "\n\n".join(f"### {p.stem}\n\n{read_if_exists(p)}" for p in log_sources)
    source_empty = not detail.strip() and not expected.strip() and not log_text.strip()
    summary = PLACEHOLDER if not log_text.strip() else "\n".join(f"- {p.stem}：见 `{p.name}`" for p in log_sources)

    data = {
        "date": args.date or datetime.now().strftime("%Y-%m-%d"),
        "issue_title": args.title or extract_table_value(detail, "问题名称") or issue_dir.name,
        "project_name": args.project or extract_table_value(detail, "所属项目") or PLACEHOLDER,
        "reporter": args.reporter or extract_table_value(detail, "反馈人") or PLACEHOLDER,
        "occurred_at": args.occurred_at or PLACEHOLDER,
        "reporter_name": args.reporter_name or PLACEHOLDER,
        "what": args.what or PLACEHOLDER,
        "why": args.why or PLACEHOLDER,
        "who": args.who or PLACEHOLDER,
        "when": args.when or PLACEHOLDER,
        "where": args.where or PLACEHOLDER,
        "how": args.how or PLACEHOLDER,
        "how_much": args.how_much or PLACEHOLDER,
        "investigation_summary": args.investigation_summary or summary,
        "temporary_solution": args.temporary_solution or PLACEHOLDER,
        "temporary_status": args.temporary_status or PLACEHOLDER,
        "permanent_solution": args.permanent_solution or PLACEHOLDER,
        "permanent_status": args.permanent_status or PLACEHOLDER,
        "validation_result": args.validation_result or PLACEHOLDER,
        "evidence_1": args.evidence_1 or "待补充问题现象截图",
        "evidence_2": args.evidence_2 or "待补充关键日志",
        "evidence_3": args.evidence_3 or "待补充验证截图",
    }
    legacy_report = issue_dir / LEGACY_REPORT_FILENAME
    report_path = issue_dir / REPORT_FILENAME if (issue_dir / REPORT_FILENAME).exists() or not legacy_report.exists() else legacy_report
    write_replace(report_path, render_template("report.md", data))
    emit(
        "success",
        action="generate_report",
        issue_directory=str(issue_dir),
        report_file=str(report_path),
        warning={"code": "ERR_REPORT_SOURCE_EMPTY", "message": "缺少可汇总来源，已生成待补充草稿。"} if source_empty else None,
    )


def list_issues(args: argparse.Namespace) -> None:
    root = Path(args.root)
    items = []
    today = datetime.now().strftime("%Y-%m-%d")
    for path in list_recent(root, args.limit):
        items.append(
            {
                "name": path.name,
                "path": str(path),
                "modified_at": datetime.fromtimestamp(path.stat().st_mtime).strftime("%Y-%m-%d %H:%M:%S"),
                "has_report": (path / REPORT_FILENAME).exists() or (path / LEGACY_REPORT_FILENAME).exists(),
                "has_today_log": (path / LOG_FILENAME).exists() or (path / LEGACY_LOG_DIRNAME / f"{today}.md").exists(),
            }
        )
    emit("success", action="list_issues", issues=items)


def archive_issue(args: argparse.Namespace) -> None:
    require_fields(args, ["project"])
    root = Path(args.root)
    issue_dir = find_issue(root, args.query, args.issue_path)
    target_root = Path(args.archive_root) / sanitize_path_part(args.project, "未命名项目") / "历史问题"
    target_root.mkdir(parents=True, exist_ok=True)
    target = target_root / issue_dir.name
    if target.exists():
        raise IssueTrackerError("ERR_DUPLICATE_ISSUE", f"归档目标已存在：{target}")
    shutil.move(str(issue_dir), str(target))
    emit("success", action="archive_issue", archived_path=str(target))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Issue tracker template generator")
    parser.add_argument("--root", default=str(DEFAULT_ROOT), help="问题记录根目录")
    parser.add_argument("--archive-root", default=str(DEFAULT_ARCHIVE_ROOT), help="项目集归档根目录")
    subparsers = parser.add_subparsers(dest="action", required=True)

    create = subparsers.add_parser("create", help="创建问题记录")
    create.add_argument("--title")
    create.add_argument("--project")
    create.add_argument("--reporter")
    create.add_argument("--symptom")
    create.add_argument("--priority", default="P2")
    create.add_argument("--initial-judgement")
    create.add_argument("--impact-scope")
    create.add_argument("--resolution-goal")
    create.set_defaults(func=create_issue)

    daily = subparsers.add_parser("daily", help="创建每日排查记录")
    daily.add_argument("--query")
    daily.add_argument("--issue-path")
    daily.add_argument("--date")
    daily.add_argument("--append", action="store_true", help="当天记录已存在时，追加新阶段而不是拒绝")
    daily.add_argument("--investigation-goal")
    daily.add_argument("--analysis-process")
    daily.add_argument("--possible-cause")
    daily.add_argument("--verification-method")
    daily.add_argument("--verification-result")
    daily.add_argument("--conclusion")
    daily.add_argument("--next-plan")
    daily.set_defaults(func=add_daily_log)

    report = subparsers.add_parser("report", help="生成总结汇报")
    report.add_argument("--query")
    report.add_argument("--issue-path")
    report.add_argument("--date")
    report.add_argument("--title")
    report.add_argument("--project")
    report.add_argument("--reporter")
    report.add_argument("--occurred-at")
    report.add_argument("--reporter-name")
    for field in ["what", "why", "who", "when", "where", "how", "how-much"]:
        report.add_argument("--" + field)
    report.add_argument("--investigation-summary")
    report.add_argument("--temporary-solution")
    report.add_argument("--temporary-status")
    report.add_argument("--permanent-solution")
    report.add_argument("--permanent-status")
    report.add_argument("--validation-result")
    report.add_argument("--evidence-1")
    report.add_argument("--evidence-2")
    report.add_argument("--evidence-3")
    report.set_defaults(func=generate_report)

    listing = subparsers.add_parser("list", help="列出近期问题")
    listing.add_argument("--limit", type=int, default=10)
    listing.set_defaults(func=list_issues)

    archive = subparsers.add_parser("archive", help="归档问题")
    archive.add_argument("--query")
    archive.add_argument("--issue-path")
    archive.add_argument("--project")
    archive.set_defaults(func=archive_issue)
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    try:
        args.func(args)
    except IssueTrackerError as error:
        fail(error)
    except PermissionError as error:
        fail(IssueTrackerError("ERR_PERMISSION_DENIED", str(error)))


if __name__ == "__main__":
    main()
