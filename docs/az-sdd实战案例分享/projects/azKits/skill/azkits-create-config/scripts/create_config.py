#!/usr/bin/env python3
"""Create an azKits YAML config and optionally its Windows launcher."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

from create_launcher import create_launcher


def response(status: str, *, data=None, code=None, message=None) -> dict:
    result = {"status": status, "timestamp": datetime.now(timezone.utc).isoformat()}
    if data is not None:
        result["data"] = data
    if code is not None:
        result["error"] = {"code": code, "message": message}
    return result


def yaml_scalar(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def create_config(output: Path, name: str, description: str, create_parent: bool) -> dict:
    if output.suffix.lower() not in {".yaml", ".yml"}:
        return response("error", code="ERR_INVALID_PATH", message="输出路径必须以 .yaml 或 .yml 结尾")
    if not name.strip() or not description.strip():
        return response("error", code="ERR_REQUIRED_INPUT", message="工具箱名称和说明不能为空")
    if len(name) > 200 or len(description) > 1000:
        return response("error", code="ERR_INVALID_TYPE", message="名称最多 200 字符，说明最多 1000 字符")
    if output.exists():
        return response("error", code="ERR_FILE_EXISTS", message=f"目标文件已存在: {output}")
    if not output.parent.exists():
        if not create_parent:
            return response("error", code="ERR_PARENT_NOT_FOUND", message=f"父目录不存在: {output.parent}")
        output.parent.mkdir(parents=True, exist_ok=True)

    content = (
        "version: 1\n"
        "toolbox:\n"
        f"  name: {yaml_scalar(name.strip())}\n"
        f"  description: {yaml_scalar(description.strip())}\n"
        "scripts: []\n"
    )
    try:
        output.write_text(content, encoding="utf-8", newline="\n")
    except PermissionError as exc:
        return response("error", code="ERR_PERMISSION_DENIED", message=str(exc))
    return response("success", data={"path": str(output.resolve()), "scripts": 0})


def main() -> int:
    parser = argparse.ArgumentParser(description="创建 azKits 空 YAML 配置")
    parser.add_argument("--output", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--description", required=True)
    parser.add_argument("--create-parent", action="store_true")
    parser.add_argument("--create-launcher", action="store_true")
    parser.add_argument("--azkits-exe")
    parser.add_argument("--launcher-name")
    args = parser.parse_args()
    result = create_config(Path(args.output), args.name, args.description, args.create_parent)
    if result["status"] == "success" and args.create_launcher:
        launcher_result = create_launcher(Path(args.output), args.azkits_exe, args.launcher_name)
        if launcher_result["status"] == "success":
            result["data"]["launcher"] = launcher_result["data"]["launcher"]
            result["data"]["azkits_exe"] = launcher_result["data"]["azkits_exe"]
        else:
            result = response(
                "partial",
                data={"path": str(Path(args.output).resolve()), "scripts": 0},
                code=launcher_result["error"]["code"],
                message="YAML 已创建，但启动器创建失败: " + launcher_result["error"]["message"],
            )
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["status"] == "success" else 1


if __name__ == "__main__":
    sys.exit(main())
