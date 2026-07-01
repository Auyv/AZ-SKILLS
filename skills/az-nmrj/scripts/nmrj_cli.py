#!/usr/bin/env python3
"""JSON wrapper around the nmrj CLI for az-nmrj."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_NMRJ_EXE = Path(r"C:\JPZ_Workspace\AZ_TOOLS\nmrj\nmrj.exe")


def now_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def decode_output(data: bytes | None) -> str:
    if not data:
        return ""
    for encoding in ("utf-8", "gbk"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def resolve_nmrj(explicit: str | None) -> str:
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    if os.environ.get("NMRJ_EXE"):
        candidates.append(Path(os.environ["NMRJ_EXE"]))
    candidates.extend([DEFAULT_NMRJ_EXE, Path("nmrj"), Path("nmrj.exe")])
    for candidate in candidates:
        if str(candidate) in {"nmrj", "nmrj.exe"}:
            return str(candidate)
        if candidate.exists():
            return str(candidate)
    raise FileNotFoundError("nmrj executable not found; set NMRJ_EXE or pass --nmrj-exe")


def emit(payload: dict, exit_code: int = 0) -> None:
    print(json.dumps(payload, ensure_ascii=False, indent=2))
    raise SystemExit(exit_code)


def run_nmrj(nmrj_exe: str, args: list[str]) -> None:
    command = [nmrj_exe] + args
    try:
        proc = subprocess.run(command, capture_output=True, timeout=60)
    except FileNotFoundError as exc:
        emit({"status": "error", "timestamp": now_iso(), "command": command,
              "error": {"code": "ERR_NMRJ_NOT_FOUND", "message": str(exc)}}, 1)
    except subprocess.TimeoutExpired:
        emit({"status": "error", "timestamp": now_iso(), "command": command,
              "error": {"code": "ERR_NMRJ_TIMEOUT", "message": "nmrj command timed out"}}, 1)

    payload = {
        "status": "success" if proc.returncode == 0 else "error",
        "timestamp": now_iso(),
        "command": command,
        "exit_code": proc.returncode,
        "stdout": decode_output(proc.stdout),
        "stderr": decode_output(proc.stderr),
    }
    if proc.returncode != 0:
        payload["error"] = {
            "code": "ERR_NMRJ_COMMAND_FAILED",
            "message": (payload["stderr"] or payload["stdout"] or "nmrj command failed").strip(),
        }
    emit(payload, proc.returncode)


def add_if_value(args: list[str], flag: str, value) -> None:
    if value is not None and value != "":
        args.extend([flag, str(value)])


def add_if_true(args: list[str], flag: str, value: bool) -> None:
    if value:
        args.append(flag)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="JSON wrapper around nmrj CLI")
    parser.add_argument("--nmrj-exe", default=None, help="Path to nmrj executable")
    sub = parser.add_subparsers(dest="action", required=True)

    p = sub.add_parser("init")
    p.add_argument("--data-dir", required=True)
    p.add_argument("--force", action="store_true")

    sub.add_parser("config-get")
    p = sub.add_parser("config-set")
    for name in ["data-dir", "attachment-dir", "backup-dir", "hook-command", "ai-command"]:
        p.add_argument(f"--{name}")

    p = sub.add_parser("task-add")
    p.add_argument("--project", required=True)
    p.add_argument("--main", required=True)
    p.add_argument("--sub", required=True)
    for name in ["nature", "type", "priority", "assignee", "status", "remark", "zentao-id"]:
        p.add_argument(f"--{name}")
    p.add_argument("--estimate", type=int)

    p = sub.add_parser("task-list")
    for name in ["project", "status", "week", "type", "nature", "priority", "assignee"]:
        p.add_argument(f"--{name}")
    p.add_argument("--output", default="json", choices=["table", "json", "csv"])

    p = sub.add_parser("task-get")
    p.add_argument("--id", required=True)
    p.add_argument("--output", default="json", choices=["table", "json", "csv"])

    p = sub.add_parser("task-update")
    p.add_argument("--id", required=True)
    for name in ["status", "actual-end", "remark", "nature", "type", "priority", "assignee", "project", "main", "sub"]:
        p.add_argument(f"--{name}")
    for name in ["progress", "current-hours", "before-hours", "estimate"]:
        p.add_argument(f"--{name}", type=int)

    p = sub.add_parser("task-delete")
    p.add_argument("--id", action="append", required=True)
    p.add_argument("--yes", "-y", action="store_true")
    p.add_argument("--delete-attachments", action="store_true")

    p = sub.add_parser("report-add")
    p.add_argument("--task", required=True)
    p.add_argument("--log", required=True)
    p.add_argument("--hours", type=int, required=True)
    p.add_argument("--progress", type=int, required=True)
    p.add_argument("--date")

    p = sub.add_parser("report-list")
    p.add_argument("--task")
    p.add_argument("--from", dest="from_date")
    p.add_argument("--to")
    p.add_argument("--output", default="json", choices=["table", "json", "csv"])

    p = sub.add_parser("report-export")
    p.add_argument("--task")
    p.add_argument("--from", dest="from_date")
    p.add_argument("--to")
    p.add_argument("--out", required=True)
    p.add_argument("--format", default="csv")
    p.add_argument("--overwrite", action="store_true")

    p = sub.add_parser("attach-add")
    p.add_argument("--task", required=True)
    p.add_argument("--file", required=True)
    p.add_argument("--overwrite", action="store_true")

    p = sub.add_parser("attach-list")
    p.add_argument("--task", required=True)

    p = sub.add_parser("attach-open")
    p.add_argument("--task", required=True)

    p = sub.add_parser("attach-delete")
    p.add_argument("--task", required=True)
    p.add_argument("--name", required=True)
    p.add_argument("--yes", "-y", action="store_true")

    p = sub.add_parser("data-backup")
    p.add_argument("--out")

    p = sub.add_parser("data-export")
    p.add_argument("--out", required=True)

    p = sub.add_parser("data-import")
    p.add_argument("--file", required=True)
    p.add_argument("--yes", "-y", action="store_true")

    p = sub.add_parser("plan-suggest")
    p.add_argument("--from", dest="from_date")
    p.add_argument("--to")
    p.add_argument("--daily-hours", type=int)
    p.add_argument("--only-active", action="store_true")
    p.add_argument("--out")
    p.add_argument("--task")

    return parser


def command_from_args(ns: argparse.Namespace) -> list[str]:
    a = ns.action
    if a == "init":
        args = ["init", "--data-dir", ns.data_dir]
        add_if_true(args, "--force", ns.force)
        return args
    if a == "config-get":
        return ["config", "get", "-o", "json"]
    if a == "config-set":
        args = ["config", "set"]
        for flag, attr in [("--data-dir", "data_dir"), ("--attachment-dir", "attachment_dir"), ("--backup-dir", "backup_dir"), ("--hook-command", "hook_command"), ("--ai-command", "ai_command")]:
            add_if_value(args, flag, getattr(ns, attr))
        return args
    if a == "task-add":
        args = ["task", "add", "--project", ns.project, "--main", ns.main, "--sub", ns.sub]
        for flag, attr in [("--nature", "nature"), ("--type", "type"), ("--priority", "priority"), ("--assignee", "assignee"), ("--status", "status"), ("--estimate", "estimate"), ("--remark", "remark"), ("--zentao-id", "zentao_id")]:
            add_if_value(args, flag, getattr(ns, attr))
        return args
    if a == "task-list":
        args = ["task", "list", "-o", ns.output]
        for flag in ["project", "status", "week", "type", "nature", "priority", "assignee"]:
            add_if_value(args, f"--{flag}", getattr(ns, flag))
        return args
    if a == "task-get":
        return ["task", "get", "--id", ns.id, "-o", ns.output]
    if a == "task-update":
        args = ["task", "update", "--id", ns.id]
        for flag, attr in [("--status", "status"), ("--progress", "progress"), ("--current-hours", "current_hours"), ("--before-hours", "before_hours"), ("--estimate", "estimate"), ("--actual-end", "actual_end"), ("--remark", "remark"), ("--nature", "nature"), ("--type", "type"), ("--priority", "priority"), ("--assignee", "assignee"), ("--project", "project"), ("--main", "main"), ("--sub", "sub")]:
            add_if_value(args, flag, getattr(ns, attr))
        return args
    if a == "task-delete":
        args = ["task", "delete"]
        for task_id in ns.id:
            args.extend(["--id", task_id])
        add_if_true(args, "--yes", ns.yes)
        add_if_true(args, "--delete-attachments", ns.delete_attachments)
        return args
    if a == "report-add":
        args = ["report", "add", "--task", ns.task, "--log", ns.log, "--hours", str(ns.hours), "--progress", str(ns.progress)]
        add_if_value(args, "--date", ns.date)
        return args
    if a == "report-list":
        args = ["report", "list", "-o", ns.output]
        add_if_value(args, "--task", ns.task)
        add_if_value(args, "--from", ns.from_date)
        add_if_value(args, "--to", ns.to)
        return args
    if a == "report-export":
        args = ["report", "export", "--out", ns.out, "--format", ns.format]
        add_if_value(args, "--task", ns.task)
        add_if_value(args, "--from", ns.from_date)
        add_if_value(args, "--to", ns.to)
        add_if_true(args, "--overwrite", ns.overwrite)
        return args
    if a == "attach-add":
        args = ["attach", "add", "--task", ns.task, "--file", ns.file]
        add_if_true(args, "--overwrite", ns.overwrite)
        return args
    if a == "attach-list":
        return ["attach", "list", "--task", ns.task]
    if a == "attach-open":
        return ["attach", "open", "--task", ns.task]
    if a == "attach-delete":
        args = ["attach", "delete", "--task", ns.task, "--name", ns.name]
        add_if_true(args, "--yes", ns.yes)
        return args
    if a == "data-backup":
        args = ["data", "backup"]
        add_if_value(args, "--out", ns.out)
        return args
    if a == "data-export":
        return ["data", "export", "--out", ns.out]
    if a == "data-import":
        args = ["data", "import", "--file", ns.file]
        add_if_true(args, "--yes", ns.yes)
        return args
    if a == "plan-suggest":
        args = ["plan", "suggest"]
        add_if_value(args, "--from", ns.from_date)
        add_if_value(args, "--to", ns.to)
        add_if_value(args, "--daily-hours", ns.daily_hours)
        add_if_true(args, "--only-active", ns.only_active)
        add_if_value(args, "--out", ns.out)
        add_if_value(args, "--task", ns.task)
        return args
    raise ValueError(f"unsupported action: {a}")


def main() -> None:
    parser = build_parser()
    ns = parser.parse_args()
    try:
        nmrj_exe = resolve_nmrj(ns.nmrj_exe)
        args = command_from_args(ns)
    except Exception as exc:
        emit({"status": "error", "timestamp": now_iso(), "error": {"code": "ERR_ARGUMENTS", "message": str(exc)}}, 1)
    run_nmrj(nmrj_exe, args)


if __name__ == "__main__":
    main()
