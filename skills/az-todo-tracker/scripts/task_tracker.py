#!/usr/bin/env python3
"""Create and maintain todo-tracker Markdown records from fixed templates.

Sibling of issue-tracker, but for forward-looking tasks (not bug investigations).
Key differences:
- Single document (README.md) holds the task, not three split files.
- progress subcommand appends "阶段 N" sections to a progress log (02_推进记录.md).
- update subcommand rewrites the status field in README.md.
- archive moves the task directory under 任务归档/.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
from datetime import datetime
from pathlib import Path


SKILL_DIR = Path(__file__).resolve().parents[1]
TEMPLATE_DIR = SKILL_DIR / "templates"

# Windows paths are intentional for Windows-shell use; WSL users MUST pass --root.
# NOTE: as of v1.0.1 these defaults were changed to WSL format because the only
# consumer of this skill is Hermes on WSL (主人 2026-06-26 decision). Windows
# PowerShell users must pass --root and --archive-root explicitly.
DEFAULT_ROOT = Path("/mnt/c/JPZ_Workspace/笔记仓库-jpz/hq-jpz/奔霄-note/任务待办")
DEFAULT_ARCHIVE_ROOT = Path("/mnt/c/JPZ_Workspace/笔记仓库-jpz/hq-jpz/奔霄-note/任务归档")

INVALID_CHARS = r'<>:"/\|?*'
PLACEHOLDER = "待补充。"
DIR_SEPARATOR = "·"
README_FILENAME = "README.md"
PROGRESS_FILENAME = "02_推进记录.md"
REPORT_FILENAME = "03_总结汇报.md"
# Status taxonomy shared with README.md "状态" field.
VALID_STATUSES = {"待启动", "进行中", "已完成", "已挂起", "已取消", "阻塞"}


class TodoTrackerError(Exception):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code
        self.message = message


def emit(status: str, **payload: object) -> None:
    print(json.dumps({"status": status, **payload}, ensure_ascii=False, indent=2))


def fail(error: TodoTrackerError) -> None:
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
        raise TodoTrackerError("ERR_TEMPLATE_NOT_FOUND", f"模板不存在：{path}")
    return path.read_text(encoding="utf-8")


def render_template(name: str, data: dict[str, str]) -> str:
    text = read_template(name)
    for key, value in data.items():
        text = text.replace("{{" + key + "}}", value)
    return re.sub(r"{{[^{}]+}}", PLACEHOLDER, text)


def write_new(path: Path, content: str) -> None:
    if path.exists():
        raise TodoTrackerError("ERR_DUPLICATE_TASK", f"文件已存在，拒绝覆盖：{path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def write_replace(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def read_if_exists(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def task_dirs(root: Path) -> list[Path]:
    if not root.exists():
        return []
    return [p for p in root.iterdir() if p.is_dir()]


def list_recent(root: Path, limit: int = 10) -> list[Path]:
    return sorted(task_dirs(root), key=lambda p: p.stat().st_mtime, reverse=True)[:limit]


def find_task(root: Path, query: str | None, task_path: str | None = None) -> Path:
    if task_path:
        path = Path(task_path)
        if not path.exists() or not path.is_dir():
            raise TodoTrackerError("ERR_TASK_NOT_FOUND", f"未找到任务目录：{path}")
        return path
    if not query:
        raise TodoTrackerError("ERR_MISSING_REQUIRED_INFO", "缺少任务名称或 task-path。")
    normalized = query.lower().strip()
    matches = [p for p in task_dirs(root) if normalized in p.name.lower()]
    if not matches:
        recent = [p.name for p in list_recent(root)]
        raise TodoTrackerError(
            "ERR_TASK_NOT_FOUND",
            "未找到匹配任务。最近记录：" + "；".join(recent),
        )
    if len(matches) > 1:
        names = "；".join(p.name for p in matches[:10])
        raise TodoTrackerError("ERR_AMBIGUOUS_TASK", "匹配到多个任务记录：" + names)
    return matches[0]


def require_fields(args: argparse.Namespace, fields: list[str]) -> None:
    missing = [field for field in fields if not getattr(args, field, None)]
    if missing:
        raise TodoTrackerError("ERR_MISSING_REQUIRED_INFO", "缺少字段：" + "、".join(missing))


# ---------------------------------------------------------------------------
# create
# ---------------------------------------------------------------------------


def create_task(args: argparse.Namespace) -> None:
    require_fields(args, ["title", "project", "owner"])
    root = Path(args.root)
    created_at = datetime.now()
    stamp = created_at.strftime("%Y-%m-%d")
    title = sanitize_path_part(args.title, "未命名任务")
    project = sanitize_path_part(args.project, "未命名项目")
    task_dir = root / DIR_SEPARATOR.join([stamp, project, title])
    if task_dir.exists():
        raise TodoTrackerError("ERR_DUPLICATE_TASK", f"任务目录已存在：{task_dir}")

    status = args.status or "待启动"
    if status not in VALID_STATUSES:
        raise TodoTrackerError(
            "ERR_INVALID_STATUS",
            f"非法状态：{status};可选:{','.join(sorted(VALID_STATUSES))}",
        )

    data = {
        "task_title": args.title,
        "project_code": args.project,
        "owner": args.owner,
        "priority": args.priority or "中",
        "status": status,
        "created_at": created_at.strftime("%Y-%m-%d %H:%M"),
        "created_at_date": stamp,
        "due_date": args.due_date or PLACEHOLDER,
        "platform": args.platform or PLACEHOLDER,
        "background": args.background or PLACEHOLDER,
        "goal": args.goal or PLACEHOLDER,
        "acceptance_1": args.acceptance_1 or PLACEHOLDER,
        "acceptance_2": args.acceptance_2 or PLACEHOLDER,
        "acceptance_3": args.acceptance_3 or PLACEHOLDER,
        "risk_1": args.risk_1 or PLACEHOLDER,
        "risk_1_impact": args.risk_1_impact or PLACEHOLDER,
        "risk_1_mitigation": args.risk_1_mitigation or PLACEHOLDER,
        "reference_1": args.reference_1 or PLACEHOLDER,
        "reference_2": args.reference_2 or PLACEHOLDER,
        "blocker_1": args.blocker_1 or PLACEHOLDER,
        "blocker_2": args.blocker_2 or PLACEHOLDER,
        "blocker_3": args.blocker_3 or PLACEHOLDER,
        "blocker_4": args.blocker_4 or PLACEHOLDER,
        "blocker_5": args.blocker_5 or PLACEHOLDER,
        "trigger_source": args.trigger_source or "主人指令",
        "subtask_1": args.subtask_1 or "（待拆分）",
        "subtask_1_status": "⏳ 未开始",
        "subtask_1_estimate": args.subtask_1_estimate or PLACEHOLDER,
        "subtask_1_actual": PLACEHOLDER,
        "subtask_2": args.subtask_2 or PLACEHOLDER,
        "subtask_2_status": "⏳ 未开始",
        "subtask_2_estimate": args.subtask_2_estimate or PLACEHOLDER,
        "subtask_2_actual": PLACEHOLDER,
        "issue_directory": str(task_dir),
    }

    write_new(task_dir / README_FILENAME, render_template("todo_detail.md", data))
    write_new(task_dir / PROGRESS_FILENAME, render_template("progress_log_placeholder.md", data))
    write_new(task_dir / REPORT_FILENAME, render_template("report_placeholder.md", data))
    notification = render_template("notification.txt", data)

    emit(
        "success",
        action="create_task",
        task_directory=str(task_dir),
        files=[
            str(task_dir / README_FILENAME),
            str(task_dir / PROGRESS_FILENAME),
            str(task_dir / REPORT_FILENAME),
        ],
        notification=notification,
    )


# ---------------------------------------------------------------------------
# progress (analogous to issue-tracker daily)
# ---------------------------------------------------------------------------


def count_phases(markdown: str) -> int:
    return max(
        [int(m) for m in re.findall(r"##\s*(?:\d{4}-\d{2}-\d{2}\s*)?阶段\s*(\d+)", markdown)] + [0]
    )


def build_phase_section(data: dict[str, str]) -> str:
    template = read_template("progress_log.md")
    body = template.split("---\n", 1)[1] if "---\n" in template else template
    for key, value in data.items():
        body = body.replace("{{" + key + "}}", value)
    return re.sub(r"{{[^{}]+}}", PLACEHOLDER, body).rstrip() + "\n"


def add_progress(args: argparse.Namespace) -> None:
    root = Path(args.root)
    task_dir = find_task(root, args.query, args.task_path)
    date = args.date or datetime.now().strftime("%Y-%m-%d")
    now = datetime.now().strftime("%H:%M")
    log_path = task_dir / PROGRESS_FILENAME

    existing = read_if_exists(log_path)
    has_phases = count_phases(existing) > 0
    if has_phases and not args.append:
        emit(
            "success",
            action="add_progress",
            task_directory=str(task_dir),
            progress_log_file=str(log_path),
            existed=True,
            hint="推进记录已存在阶段，需要追加新阶段请加 --append",
        )
        return

    phase = (count_phases(existing) + 1) if has_phases else 1
    data = {
        "date": date,
        "phase": str(phase),
        "phase_timestamp": now,
        "action": args.action_text or PLACEHOLDER,
        "result": args.result or PLACEHOLDER,
        "blocker": args.blocker or PLACEHOLDER,
        "next_plan": args.next_plan or PLACEHOLDER,
    }
    section = build_phase_section(data)

    if existing.strip():
        base = existing.rstrip()
        if not has_phases:
            base = re.sub(r"\n*暂无推进记录。\s*$", "", base).rstrip()
        separator = "\n\n---\n\n" if not base.endswith("\n\n") else "\n---\n\n"
        write_replace(log_path, base + separator + section)
    else:
        header = (
            "# 推进记录\n\n"
            "> 同一文件内可按\"阶段 1/2/3...\"追加多次推进;每次追加会自动写入当前日期 + 时间戳并生成新阶段小节。\n"
            "> 文件已有阶段时默认拒绝覆盖,需要追加请加 `--append`。\n\n---\n\n"
        )
        write_new(log_path, header + section)

    emit(
        "success",
        action="add_progress",
        task_directory=str(task_dir),
        progress_log_file=str(log_path),
        existed=False,
        phase=phase,
        phase_timestamp=now,
    )


# ---------------------------------------------------------------------------
# update (status / priority / etc.)
# ---------------------------------------------------------------------------


def extract_table_value(markdown: str, field: str) -> str:
    pattern = re.compile(rf"\|\s*{re.escape(field)}\s*\|\s*(.*?)\s*\|")
    for line in markdown.splitlines():
        m = pattern.search(line)
        if m:
            return m.group(1).strip()
    return ""


def replace_table_value(markdown: str, field: str, new_value: str) -> str:
    pattern = re.compile(rf"(\|\s*{re.escape(field)}\s*\|\s*)(.*?)(\s*\|)")
    new_lines = []
    replaced = False
    for line in markdown.splitlines():
        m = pattern.search(line)
        if m and not replaced:
            new_lines.append(f"{m.group(1)}{new_value}{m.group(3)}")
            replaced = True
        else:
            new_lines.append(line)
    if not replaced:
        raise TodoTrackerError(
            "ERR_FIELD_NOT_FOUND",
            f"在 README.md 顶部信息表中未找到字段:{field}",
        )
    return "\n".join(new_lines) + ("\n" if markdown.endswith("\n") else "")


def update_task(args: argparse.Namespace) -> None:
    root = Path(args.root)
    task_dir = find_task(root, args.query, args.task_path)
    readme = task_dir / README_FILENAME
    if not readme.exists():
        raise TodoTrackerError("ERR_TASK_NOT_FOUND", f"未找到 README.md:{readme}")
    content = read_if_exists(readme)
    changes: list[str] = []

    if args.status:
        if args.status not in VALID_STATUSES:
            raise TodoTrackerError(
                "ERR_INVALID_STATUS",
                f"非法状态:{args.status};可选:{','.join(sorted(VALID_STATUSES))}",
            )
        content = replace_table_value(content, "状态", args.status)
        changes.append(f"状态 → {args.status}")
    if args.priority:
        content = replace_table_value(content, "优先级", args.priority)
        changes.append(f"优先级 → {args.priority}")
    if args.due_date:
        content = replace_table_value(content, "截止日期", args.due_date)
        changes.append(f"截止日期 → {args.due_date}")
    if args.append_progress_row:
        stamp = datetime.now().strftime("%Y-%m-%d")
        row = f"| {stamp} | {args.append_progress_row} | {args.status or extract_table_value(content, '状态') or '已更新'} | 主人 update 子命令 |"
        # append into section 8 (推进记录) — find the last "| date | action |" row in that table.
        if "| 日期 | 动作 | 状态变更 | 备注 |" not in content:
            raise TodoTrackerError(
                "ERR_FIELD_NOT_FOUND",
                "README.md 中未找到推进记录表格,无法追加",
            )
        # Insert before the first '---' after the table header.
        lines = content.splitlines()
        insert_at = None
        seen_header = False
        for idx, line in enumerate(lines):
            if line.strip().startswith("| 日期 | 动作 | 状态变更 | 备注 |"):
                seen_header = True
                continue
            if seen_header and line.strip().startswith("|---") or (seen_header and line.strip() == "---"):
                insert_at = idx
                break
        if insert_at is None:
            # No trailing separator; just append at end.
            content = content.rstrip() + "\n" + row + "\n"
        else:
            lines.insert(insert_at, row)
            content = "\n".join(lines) + "\n"
        changes.append(f"追加推进记录:{args.append_progress_row}")

    if not changes:
        raise TodoTrackerError(
            "ERR_MISSING_REQUIRED_INFO",
            "至少需要传入 --status / --priority / --due-date / --append-progress-row 之一",
        )

    write_replace(readme, content)
    emit(
        "success",
        action="update_task",
        task_directory=str(task_dir),
        readme=str(readme),
        changes=changes,
    )


# ---------------------------------------------------------------------------
# report / list / archive
# ---------------------------------------------------------------------------


def generate_report(args: argparse.Namespace) -> None:
    root = Path(args.root)
    task_dir = find_task(root, args.query, args.task_path)
    readme = read_if_exists(task_dir / README_FILENAME)
    progress = read_if_exists(task_dir / PROGRESS_FILENAME)
    report_path = task_dir / REPORT_FILENAME

    data = {
        "date": args.date or datetime.now().strftime("%Y-%m-%d"),
        "task_title": args.title or extract_table_value(readme, "任务名称") or task_dir.name,
        "project_code": args.project or extract_table_value(readme, "项目编号") or PLACEHOLDER,
        "owner": args.owner or extract_table_value(readme, "负责人") or PLACEHOLDER,
        "started_at": args.started_at or extract_table_value(readme, "创建时间") or PLACEHOLDER,
        "completed_at": args.completed_at or datetime.now().strftime("%Y-%m-%d"),
        "actual_effort": args.actual_effort or PLACEHOLDER,
        "goal": args.goal or PLACEHOLDER,
        "subtask_1": args.subtask_1 or PLACEHOLDER,
        "subtask_1_estimate": args.subtask_1_estimate or PLACEHOLDER,
        "subtask_1_actual": args.subtask_1_actual or PLACEHOLDER,
        "subtask_1_completion": args.subtask_1_completion or PLACEHOLDER,
        "acceptance_1": args.acceptance_1 or PLACEHOLDER,
        "acceptance_2": args.acceptance_2 or PLACEHOLDER,
        "lessons_learned": args.lessons_learned or PLACEHOLDER,
        "follow_ups": args.follow_ups or PLACEHOLDER,
    }
    # If user has not provided 推进记录 excerpt, drop a small hint into lessons_learned.
    if not progress.strip() or "暂无推进记录" in progress:
        data["lessons_learned"] = "⚠️ 02_推进记录.md 为空,缺少过程数据。" + data["lessons_learned"]

    source_empty = not readme.strip() and not progress.strip()
    write_replace(report_path, render_template("report.md", data))
    emit(
        "success",
        action="generate_report",
        task_directory=str(task_dir),
        report_file=str(report_path),
        warning=(
            {"code": "ERR_REPORT_SOURCE_EMPTY", "message": "缺少可汇总来源,已生成待补充草稿。"}
            if source_empty
            else None
        ),
    )


def list_tasks(args: argparse.Namespace) -> None:
    root = Path(args.root)
    items = []
    for path in list_recent(root, args.limit):
        readme = path / README_FILENAME
        status = extract_table_value(read_if_exists(readme), "状态") if readme.exists() else ""
        items.append(
            {
                "name": path.name,
                "path": str(path),
                "status": status or "未知",
                "has_report": (path / REPORT_FILENAME).exists(),
                "has_progress": count_phases(read_if_exists(path / PROGRESS_FILENAME)) > 0,
                "modified_at": datetime.fromtimestamp(path.stat().st_mtime).strftime(
                    "%Y-%m-%d %H:%M:%S"
                ),
            }
        )
    emit("success", action="list_tasks", tasks=items)


def archive_task(args: argparse.Namespace) -> None:
    root = Path(args.root)
    task_dir = find_task(root, args.query, args.task_path)
    archive_root = Path(args.archive_root)
    target = archive_root / task_dir.name
    if target.exists():
        raise TodoTrackerError("ERR_DUPLICATE_TASK", f"归档目标已存在：{target}")
    archive_root.mkdir(parents=True, exist_ok=True)
    shutil.move(str(task_dir), str(target))
    emit("success", action="archive_task", archived_path=str(target))


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Todo tracker template generator")
    parser.add_argument("--root", default=str(DEFAULT_ROOT), help="任务待办根目录")
    parser.add_argument("--archive-root", default=str(DEFAULT_ARCHIVE_ROOT), help="归档根目录")
    subparsers = parser.add_subparsers(dest="action", required=True)

    # create
    create = subparsers.add_parser("create", help="创建新任务")
    create.add_argument("--title", help="任务名称(一句话简述)")
    create.add_argument("--project", help="项目编号")
    create.add_argument("--owner", help="负责人")
    create.add_argument("--priority", default="中", help="高/中/低,默认 中")
    create.add_argument(
        "--status", default="待启动", help="待启动/进行中/已完成/已挂起/已取消/阻塞"
    )
    create.add_argument("--due-date", help="截止日期 YYYY-MM-DD")
    create.add_argument("--platform", help="涉及平台/模组")
    create.add_argument("--background", help="背景说明")
    create.add_argument("--goal", help="目标说明")
    create.add_argument("--acceptance-1")
    create.add_argument("--acceptance-2")
    create.add_argument("--acceptance-3")
    create.add_argument("--risk-1")
    create.add_argument("--risk-1-impact")
    create.add_argument("--risk-1-mitigation")
    create.add_argument("--reference-1")
    create.add_argument("--reference-2")
    create.add_argument("--blocker-1")
    create.add_argument("--blocker-2")
    create.add_argument("--blocker-3")
    create.add_argument("--blocker-4")
    create.add_argument("--blocker-5")
    create.add_argument("--trigger-source", help="触发来源,默认 主人指令")
    create.add_argument("--subtask-1")
    create.add_argument("--subtask-1-estimate")
    create.add_argument("--subtask-2")
    create.add_argument("--subtask-2-estimate")
    create.set_defaults(func=create_task)

    # progress
    progress = subparsers.add_parser("progress", help="追加推进记录")
    progress.add_argument("--query")
    progress.add_argument("--task-path")
    progress.add_argument("--date")
    progress.add_argument("--append", action="store_true", help="已有阶段时追加新阶段")
    progress.add_argument("--action-text", help="本阶段做了什么")
    progress.add_argument("--result", help="完成情况")
    progress.add_argument("--blocker", help="卡点 / 决策")
    progress.add_argument("--next-plan", help="下一步计划")
    progress.set_defaults(func=add_progress)

    # update
    update = subparsers.add_parser("update", help="更新任务状态/优先级/截止日期")
    update.add_argument("--query")
    update.add_argument("--task-path")
    update.add_argument("--status", help="新状态")
    update.add_argument("--priority", help="新优先级")
    update.add_argument("--due-date", help="新截止日期")
    update.add_argument(
        "--append-progress-row", help="在 README 第 8 节追加一行推进记录(动作描述)"
    )
    update.set_defaults(func=update_task)

    # report
    report = subparsers.add_parser("report", help="生成任务总结汇报")
    report.add_argument("--query")
    report.add_argument("--task-path")
    report.add_argument("--date")
    report.add_argument("--title")
    report.add_argument("--project")
    report.add_argument("--owner")
    report.add_argument("--started-at")
    report.add_argument("--completed-at")
    report.add_argument("--actual-effort")
    report.add_argument("--goal")
    report.add_argument("--subtask-1")
    report.add_argument("--subtask-1-estimate")
    report.add_argument("--subtask-1-actual")
    report.add_argument("--subtask-1-completion")
    report.add_argument("--acceptance-1")
    report.add_argument("--acceptance-2")
    report.add_argument("--lessons-learned")
    report.add_argument("--follow-ups")
    report.set_defaults(func=generate_report)

    # list
    listing = subparsers.add_parser("list", help="列出近期任务")
    listing.add_argument("--limit", type=int, default=10)
    listing.set_defaults(func=list_tasks)

    # archive
    archive = subparsers.add_parser("archive", help="归档已完成/已终止任务")
    archive.add_argument("--query")
    archive.add_argument("--task-path")
    archive.set_defaults(func=archive_task)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    try:
        args.func(args)
    except TodoTrackerError as error:
        fail(error)
    except PermissionError as error:
        fail(TodoTrackerError("ERR_PERMISSION_DENIED", str(error)))


if __name__ == "__main__":
    main()
