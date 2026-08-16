#!/usr/bin/env python3
"""Create a one-click Windows launcher beside an azKits YAML config."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path


def response(status: str, *, data=None, code=None, message=None) -> dict:
    result = {"status": status, "timestamp": datetime.now(timezone.utc).isoformat()}
    if data is not None:
        result["data"] = data
    if code is not None:
        result["error"] = {"code": code, "message": message}
    return result


def discover_azkits_exe(config: Path, explicit: str | None) -> Path | None:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(os.path.expandvars(explicit)))
    else:
        candidates.append(config.parent / "azkits.exe")
        if os.environ.get("AZKITS_EXE"):
            candidates.append(Path(os.path.expandvars(os.environ["AZKITS_EXE"])))
        candidates.append(Path(__file__).resolve().parents[3] / "dist" / "azkits.exe")
        found = shutil.which("azkits.exe")
        if found:
            candidates.append(Path(found))

    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if resolved.is_file() and resolved.suffix.lower() == ".exe":
            return resolved
    return None


def batch_value(value: str) -> str:
    return value.replace("%", "%%")


def create_launcher(config: Path, azkits_exe: str | None, launcher_name: str | None) -> dict:
    if config.suffix.lower() not in {".yaml", ".yml"}:
        return response("error", code="ERR_INVALID_PATH", message="配置路径必须以 .yaml 或 .yml 结尾")
    if not config.is_file():
        return response("error", code="ERR_CONFIG_NOT_FOUND", message=f"配置不存在: {config}")

    name = launcher_name or f"{config.stem}Run.bat"
    if not name.lower().endswith(".bat"):
        name += ".bat"
    if Path(name).name != name or Path(name).suffix.lower() != ".bat":
        return response("error", code="ERR_INVALID_LAUNCHER_NAME", message="启动器名称只能是同级 .bat 文件名")

    launcher = config.with_name(name)
    if launcher.exists():
        return response("error", code="ERR_LAUNCHER_EXISTS", message=f"启动器已存在: {launcher}")

    executable = discover_azkits_exe(config, azkits_exe)
    if executable is None:
        return response(
            "error",
            code="ERR_AZKITS_NOT_FOUND",
            message="未找到 azkits.exe；请使用 --azkits-exe 指定路径，或将 EXE 放到配置同级目录",
        )

    content = f'''@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "AZKITS_EXE={batch_value(str(executable))}"
set "AZKITS_CONFIG=%~dp0{batch_value(config.name)}"

if not exist "%AZKITS_EXE%" (
    echo ERROR: azKits executable not found:
    echo %AZKITS_EXE%
    pause
    exit /b 1
)

if not exist "%AZKITS_CONFIG%" (
    echo ERROR: azKits config not found:
    echo %AZKITS_CONFIG%
    pause
    exit /b 2
)

"%AZKITS_EXE%" --config "%AZKITS_CONFIG%"
set "AZKITS_EXIT_CODE=%ERRORLEVEL%"

if not "%AZKITS_EXIT_CODE%"=="0" (
    echo.
    echo ERROR: azKits exited with code %AZKITS_EXIT_CODE%.
    pause
)

exit /b %AZKITS_EXIT_CODE%
'''
    try:
        launcher.write_text(content, encoding="utf-8", newline="\r\n")
    except PermissionError as exc:
        return response("error", code="ERR_PERMISSION_DENIED", message=str(exc))
    return response(
        "success",
        data={"launcher": str(launcher.resolve()), "config": str(config.resolve()), "azkits_exe": str(executable)},
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="在 azKits YAML 同级创建一键启动 BAT")
    parser.add_argument("--config", required=True)
    parser.add_argument("--azkits-exe")
    parser.add_argument("--launcher-name")
    args = parser.parse_args()
    result = create_launcher(Path(args.config), args.azkits_exe, args.launcher_name)
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["status"] == "success" else 1


if __name__ == "__main__":
    sys.exit(main())
