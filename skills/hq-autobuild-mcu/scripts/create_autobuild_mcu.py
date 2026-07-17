#!/usr/bin/env python3
"""Create a Windows autobuild-mcu.bat for local MCU CI repository debugging."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path


SAFE_VALUE = re.compile(r"^[A-Za-z0-9._-]+$")


class SkillError(Exception):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


def response(status: str, data=None, code=None, message=None) -> dict:
    result = {"status": status, "timestamp": datetime.now(timezone.utc).isoformat()}
    if data is not None:
        result["data"] = data
    if code is not None:
        result["error"] = {"code": code, "message": message}
    return result


def validate_value(name: str, value: str) -> None:
    error = None
    if not isinstance(value, str) or not value:
        error = f"{name} must be a non-empty string"
    elif len(value) > 4096:
        error = f"{name} exceeds 4096 characters"
    elif name != "repository_root" and name != "output_path" and not SAFE_VALUE.fullmatch(value):
        error = f"{name} may contain only letters, digits, dot, underscore, and hyphen"
    if error is not None:
        raise SkillError("ERR_INVALID_INPUT", error)


def resolve_project(root: Path, requested: str | None) -> str:
    project = requested
    if project is not None:
        validate_value("project_name", project)
        if not (root / project / "build" / "mcu").is_dir():
            raise SkillError("ERR_PROJECT_NOT_FOUND", f"project build/mcu not found: {root / project / 'build' / 'mcu'}")
    else:
        candidates = sorted(p.parent.parent.name for p in root.glob("*/build/mcu") if p.is_dir())
        if len(candidates) != 1:
            raise SkillError("ERR_PROJECT_NOT_FOUND", f"expected one build/mcu project, found {len(candidates)}: {candidates}")
        project = candidates[0]
    return project


def render(project: str, product: str, model: str, mode: str) -> str:
    calls = []
    if mode == "build-pack":
        calls.append(("build.bat", product))
    calls.append(("pack.bat", product))
    call_lines = []
    for script, argument in calls:
        call_lines.extend([
            f'call "{script}" "{argument}"',
            "if errorlevel 1 (",
            "    set \"RC=!errorlevel!\"",
            f'    echo Error: {script} failed with error code !RC!',
            "    popd",
            "    exit /b !RC!",
            ")",
            "",
        ])
    body = "\r\n".join(call_lines).rstrip()
    return (
        ":: file        autobuild-mcu.bat\r\n"
        ":: brief       Local MCU CI repository debug entry\r\n"
        ":: generated   by hq-autobuild-mcu\r\n\r\n"
        "@echo off\r\n"
        "setlocal EnableExtensions EnableDelayedExpansion\r\n\r\n"
        "set \"SCRIPT_DIR=%~dp0\"\r\n"
        f"set \"G_PROJECT_NAME={project}\"\r\n"
        f"set \"G_PRODUCT_NAME={product}\"\r\n"
        f"set \"G_BUILD_MODEL={model}\"\r\n\r\n"
        "echo ============= BEGIN: autobuild-mcu.bat =============\r\n"
        "echo SCRIPT_DIR=!SCRIPT_DIR!\r\n"
        "echo G_PROJECT_NAME=!G_PROJECT_NAME!\r\n"
        "echo G_PRODUCT_NAME=!G_PRODUCT_NAME!\r\n"
        "echo G_BUILD_MODEL=!G_BUILD_MODEL!\r\n\r\n"
        "pushd \"!SCRIPT_DIR!!G_PROJECT_NAME!\\build\\mcu\"\r\n"
        "if errorlevel 1 (\r\n"
        "    echo Error: cannot enter MCU build directory\r\n"
        "    exit /b 2\r\n"
        ")\r\n\r\n"
        f"{body}\r\n\r\n"
        "popd\r\n"
        "echo ============= END: autobuild-mcu.bat =============\r\n"
        "endlocal\r\n"
        "exit /b 0\r\n"
    )


def create(args: argparse.Namespace) -> dict:
    validate_value("repository_root", args.repository_root)
    validate_value("product_name", args.product_name)
    validate_value("build_model", args.build_model)
    root = Path(args.repository_root).expanduser().resolve()
    if not root.is_dir():
        raise SkillError("ERR_REPOSITORY_NOT_FOUND", f"repository root not found: {root}")
    project = resolve_project(root, args.project_name)
    build_dir = root / project / "build" / "mcu"
    required = [build_dir / "pack.bat"]
    if args.mode == "build-pack":
        required.insert(0, build_dir / "build.bat")
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise SkillError("ERR_BUILD_ENTRY_NOT_FOUND", f"missing build entries: {missing}")
    output = Path(args.output_path).expanduser().resolve() if args.output_path else root / "autobuild-mcu.bat"
    if output.parent != root:
        raise SkillError("ERR_INVALID_INPUT", f"output must be located in repository root: {root}")
    if output.exists() and not args.force:
        raise SkillError("ERR_OUTPUT_EXISTS", f"output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    backup = None
    if output.exists():
        backup = output.with_suffix(output.suffix + ".bak")
        shutil.copy2(output, backup)
    output.write_text(render(project, args.product_name, args.build_model, args.mode), encoding="utf-8-sig", newline="")
    return response("success", {"output_path": str(output), "backup_path": str(backup) if backup else None, "project_name": project, "mode": args.mode})


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository-root", required=True)
    parser.add_argument("--product-name", required=True)
    parser.add_argument("--project-name")
    parser.add_argument("--build-model", default="mcu")
    parser.add_argument("--mode", choices=("pack", "build-pack"), default="pack")
    parser.add_argument("--output-path")
    parser.add_argument("--force", action="store_true")
    return parser.parse_args()


def main() -> int:
    exit_code = 0
    try:
        result = create(parse_args())
    except SkillError as exc:
        result = response("error", code=exc.code, message=str(exc))
        exit_code = 2
    except PermissionError as exc:
        result = response("error", code="ERR_PERMISSION_DENIED", message=str(exc))
        exit_code = 3
    except (OSError, ValueError, TypeError) as exc:
        result = response("error", code="ERR_INVALID_INPUT", message=str(exc))
        exit_code = 4
    print(json.dumps(result, ensure_ascii=False))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
