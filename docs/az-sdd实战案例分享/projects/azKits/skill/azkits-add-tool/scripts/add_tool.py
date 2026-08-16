#!/usr/bin/env python3
"""Append one canonical tool entry to an azKits YAML file."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

TYPES = {"auto", "bat", "cmd", "powershell", "python", "shell", "exe"}
ENCODINGS = {"utf-8", "utf-8-sig", "gbk", "gb18030"}


def response(status: str, *, data=None, code=None, message=None) -> dict:
    result = {"status": status, "timestamp": datetime.now(timezone.utc).isoformat()}
    if data is not None:
        result["data"] = data
    if code is not None:
        result["error"] = {"code": code, "message": message}
    return result


def scalar(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def decode_scalar(value: str) -> str:
    value = value.strip()
    if value.startswith(('"', "'")):
        try:
            return str(json.loads(value))
        except json.JSONDecodeError:
            return value.strip("'\"")
    return value.split(" #", 1)[0].strip()


def existing_values(text: str, field: str) -> list[str]:
    pattern = re.compile(rf"^\s+(?:-\s+)?{re.escape(field)}:\s*(.*?)\s*$", re.MULTILINE)
    return [decode_scalar(match.group(1)) for match in pattern.finditer(text)]


def parse_env(items: list[str]) -> tuple[dict[str, str], str | None]:
    values: dict[str, str] = {}
    for item in items:
        if "=" not in item or not item.split("=", 1)[0]:
            return {}, f"环境变量必须使用 KEY=VALUE: {item}"
        key, value = item.split("=", 1)
        values[key] = value
    return values, None


def render_entry(args: argparse.Namespace, env: dict[str, str]) -> str:
    lines = [
        f"  - id: {scalar(args.id)}",
        f"    alias: {scalar(args.alias)}",
        f"    name: {scalar(args.name)}",
        f"    description: {scalar(args.description)}",
        f"    path: {scalar(args.path)}",
        f"    type: {args.type}",
        f"    enabled: {'false' if args.disabled else 'true'}",
    ]
    if args.arg:
        lines.append("    args: [" + ", ".join(scalar(item) for item in args.arg) + "]")
    if args.cwd:
        lines.append(f"    cwd: {scalar(args.cwd)}")
    if env:
        lines.append("    env:")
        lines.extend(f"      {scalar(key)}: {scalar(value)}" for key, value in env.items())
    lines.append(f"    encoding: {args.encoding}")
    return "\n".join(lines) + "\n"


def add_tool(args: argparse.Namespace) -> dict:
    config = Path(args.config)
    required = {"id": args.id, "alias": args.alias, "name": args.name,
                "description": args.description, "path": args.path}
    missing = [key for key, value in required.items() if not isinstance(value, str) or not value.strip()]
    if missing:
        return response("error", code="ERR_REQUIRED_INPUT", message="字段不能为空: " + ", ".join(missing))
    if config.suffix.lower() not in {".yaml", ".yml"}:
        return response("error", code="ERR_INVALID_CONFIG", message="配置必须是 .yaml 或 .yml")
    if not config.is_file():
        return response("error", code="ERR_FILE_NOT_FOUND", message=f"配置不存在: {config}")
    if args.type not in TYPES or args.encoding not in ENCODINGS:
        return response("error", code="ERR_INVALID_TYPE", message="type 或 encoding 不在 azKits 支持列表中")
    if any(len(required[key]) > limit for key, limit in {"id": 128, "alias": 128, "name": 200, "description": 1000}.items()):
        return response("error", code="ERR_INVALID_TYPE", message="id/alias/name/description 超过长度限制")
    env, env_error = parse_env(args.env)
    if env_error:
        return response("error", code="ERR_INVALID_TYPE", message=env_error)

    try:
        text = config.read_text(encoding="utf-8-sig")
    except UnicodeDecodeError as exc:
        return response("error", code="ERR_INVALID_CONFIG", message=f"配置不是 UTF-8: {exc}")
    if not re.search(r"(?m)^version:\s*1\s*$", text) or not re.search(r"(?m)^toolbox:\s*$", text):
        return response("error", code="ERR_INVALID_CONFIG", message="缺少 version: 1 或 toolbox 根字段")
    scripts_match = re.search(r"(?m)^scripts:\s*(\[\])?\s*$", text)
    if not scripts_match:
        return response("error", code="ERR_INVALID_CONFIG", message="缺少受支持的 scripts 根字段")
    if args.id in existing_values(text, "id"):
        return response("error", code="ERR_DUPLICATE_ID", message=f"id 已存在: {args.id}")
    aliases = {value.casefold() for value in existing_values(text, "alias")}
    if args.alias.casefold() in aliases:
        return response("error", code="ERR_DUPLICATE_ALIAS", message=f"alias 已存在: {args.alias}")

    entry = render_entry(args, env)
    if scripts_match.group(1):
        tail = text[scripts_match.end():]
        updated = text[:scripts_match.start()] + "scripts:\n" + entry + tail.lstrip("\r\n")
    else:
        next_root = re.search(r"(?m)^\S", text[scripts_match.end():])
        insert_at = scripts_match.end() + (next_root.start() if next_root else len(text[scripts_match.end():]))
        prefix = text[:insert_at].rstrip()
        suffix = text[insert_at:].lstrip("\r\n")
        updated = prefix + "\n" + entry + suffix
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    backup = config.with_name(config.name + f".azkits-add-tool.{stamp}.bak")
    try:
        shutil.copy2(config, backup)
        config.write_text(updated, encoding="utf-8", newline="\n")
    except PermissionError as exc:
        return response("error", code="ERR_PERMISSION_DENIED", message=str(exc))
    return response("success", data={"config": str(config.resolve()), "backup": str(backup.resolve()),
                                     "id": args.id, "alias": args.alias})


def main() -> int:
    parser = argparse.ArgumentParser(description="向 azKits YAML 添加一个工具描述")
    parser.add_argument("--config", required=True)
    parser.add_argument("--id", required=True)
    parser.add_argument("--alias", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--description", required=True)
    parser.add_argument("--path", required=True)
    parser.add_argument("--type", choices=sorted(TYPES), default="auto")
    parser.add_argument("--disabled", action="store_true")
    parser.add_argument("--arg", action="append", default=[])
    parser.add_argument("--cwd", default="")
    parser.add_argument("--env", action="append", default=[])
    parser.add_argument("--encoding", choices=sorted(ENCODINGS), default="utf-8")
    args = parser.parse_args()
    result = add_tool(args)
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["status"] == "success" else 1


if __name__ == "__main__":
    sys.exit(main())
