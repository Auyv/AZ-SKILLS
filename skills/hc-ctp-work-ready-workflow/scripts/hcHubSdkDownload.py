#!/usr/bin/env python3
"""Download and unpack the HUBS prebuilt SDK package for a CTP product.

The download zip and unpacked staging tree land in ``<hubs-cache-dir>/downloads``
and ``<hubs-cache-dir>/staging_<product>/`` (default ``<workspace>/.ai_tmp/hubs-sdk/``).
The build script then reads the SDK from ``<workspace>/sdk/<build_model>/``,
which is copied from staging. Intermediate artefacts are NOT cleaned up
automatically; remove ``.ai_tmp/`` manually when no longer needed.
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_BASE_URL = "http://172.16.1.99:8081/artifactory/sei/project"
DEFAULT_BUILD_MODEL = "ag35_ceraa"
DEFAULT_MCU_BUILD_MODEL = "s32k312"
DEFAULT_HUBS_CACHE_SUBDIR = ".ai_tmp/hubs-sdk"

HUBS_KEYS = (
    "G_PROJ_HUBS_PRODUCT_NAME",
    "G_PROJ_HUBS_PCB_VERSION",
    "G_PROJ_HUBS_BOOM_VERSION",
    "G_PROJ_HUBS_SOFT_VERSION",
)

NEED_HUBS_LIST_KEYS = (
    "G_PROJ_LIST_HUBS_LIB",
    "G_PROJ_LIST_HUBS_SO",
    "G_PROJ_LIST_HUBS_PLUG",
    "G_PROJ_LIST_HUBS_BIN",
    "G_PROJ_LIST_HUBS_DRV",
)

CRED_KEYS = (
    "G_PROJ_HUBS_ARTIFACTORY_CRED",
    "G_PROJ_HUBS_SDK_CRED",
    "G_PROJ_ARTIFACTORY_CRED",
    "G_HUBS_ARTIFACTORY_CRED",
)

SAFE_VALUE = re.compile(r"^[A-Za-z0-9._-]+$")
EXPORT_LINE = re.compile(r'^\s*export\s+([A-Za-z_][A-Za-z0-9_]*)="?([^"]*)"?\s*$')
CURL_CRED = re.compile(r"curl\s+-u(\S+)")


class SkillError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def response(status, data=None, code=None, message=None):
    result = {"status": status, "timestamp": datetime.now(timezone.utc).isoformat()}
    if data is not None:
        result["data"] = data
    if code is not None:
        result["error"] = {"code": code, "message": message}
    return result


def emit(payload):
    print(payload["status"].upper())
    data = payload.get("data")
    if isinstance(data, dict):
        for key, value in data.items():
            print(f"{key}: {value}")
    error = payload.get("error")
    if error:
        print(f"error[{error['code']}]: {error['message']}")


def parse_list_env(env_file):
    if not env_file.is_file():
        raise SkillError("Error_EnvNotFound", f"list.env not found: {env_file}")
    text = None
    for encoding in ("utf-8", "gbk", "latin-1"):
        try:
            text = env_file.read_text(encoding=encoding)
            break
        except (UnicodeDecodeError, LookupError):
            continue
    if text is None:
        raise SkillError("Error_EnvEncoding", f"cannot decode list.env: {env_file}")
    values = {}
    for raw in text.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        matched = EXPORT_LINE.match(raw)
        if matched:
            values[matched.group(1)] = matched.group(2).strip()
    return values


def need_hubs_sdk(values):
    return any(values.get(key, "").strip() for key in NEED_HUBS_LIST_KEYS)


def build_package_names(product, pcb, boom, soft):
    return f"{product}_{pcb}{boom}.zip", f"{product}_{pcb}{boom}_{soft}.zip"


def build_urls(base_url, product, pcb, boom, soft, pinned):
    latest, pinned_name = build_package_names(product, pcb, boom, soft)
    root = f"{base_url.rstrip('/')}/{product}/{pcb}{boom}"
    if pinned:
        return pinned_name, f"{root}/history/{pinned_name}"
    return latest, f"{root}/{latest}"


def resolve_credential(explicit, env_values, workspace):
    for key in CRED_KEYS:
        found = env_values.get(key, "").strip()
        if found:
            return found, f"list.env:{key}"
    from_env = os.environ.get("HUBS_ARTIFACTORY_CRED", "").strip()
    if from_env:
        return from_env, "env:HUBS_ARTIFACTORY_CRED"
    for candidate in sorted(workspace.glob("*/*/build/mpu/build_ag35_ceraa.sh")):
        for line in candidate.read_text(encoding="utf-8", errors="replace").splitlines():
            found = CURL_CRED.search(line)
            if found:
                return found.group(1), f"build-script:{candidate.name}"
    if explicit:
        return explicit, "--cred"
    raise SkillError(
        "Error_CredMissing",
        f"no Artifactory credential: add one of {list(CRED_KEYS)} to list.env, "
        "set HUBS_ARTIFACTORY_CRED, pass --cred, or run inside a workspace "
        "that contains build/mpu/build_ag35_ceraa.sh",
    )


def download(url, credential, target):
    command = ["curl", "-f", "-s", "-S", "-u", credential, "-o", str(target), url]
    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "").strip()
        raise SkillError("Error_Download", f"curl exit {completed.returncode}: {detail}")
    if not target.is_file() or target.stat().st_size == 0:
        raise SkillError("Error_Download", f"downloaded file is empty: {target}")


def unpack(archive, staging):
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    with zipfile.ZipFile(archive) as package:
        package.extractall(staging)
    if not (staging / "sdk").is_dir():
        raise SkillError("Error_Unpack", f"archive has no sdk/ directory: {archive}")
    return staging


def place_sdk(staging, source_subdir, target):
    """Copy ``<staging>/<source_subdir>`` subtree into ``target``.

    ``source_subdir`` is relative to ``staging``; e.g. ``"sdk/ag35_ceraa"``.
    The contents of that source are flattened into ``target`` (matching
    ``build_ag35_ceraa.sh``'s ``cp -rf staging/sdk/ag35_ceraa sdk``).
    """
    source = staging / source_subdir
    if not source.is_dir():
        raise SkillError(
            "Error_BuildModel",
            f"source path not found: {source}",
        )
    if target.exists():
        shutil.rmtree(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, target)
    return target


def expected_layouts(sdk_dir):
    return {name: (sdk_dir / name).is_dir() for name in ("bin", "lib", "plug", "ko", "env")}


def default_workspace(env_file):
    parts = env_file.resolve().parts
    if len(parts) < 5:
        return Path.cwd()
    return Path(*parts[:-5])


def default_platform(env_file):
    """`<ws>/<platform>/<repo>/config/<product>/list.env` -> `<platform>`."""
    parts = env_file.resolve().parts
    if len(parts) < 5:
        return None
    return parts[-5]


def run(args):
    env_file = Path(args.list_env)
    values = parse_list_env(env_file)

    missing = [key for key in HUBS_KEYS if not values.get(key, "").strip()]
    if missing:
        raise SkillError("Error_HubsKeys", f"list.env lacks {missing}")

    product = values["G_PROJ_HUBS_PRODUCT_NAME"]
    pcb = values["G_PROJ_HUBS_PCB_VERSION"]
    boom = values["G_PROJ_HUBS_BOOM_VERSION"]
    soft = values["G_PROJ_HUBS_SOFT_VERSION"]

    for label, item in (("product", product), ("pcb", pcb), ("boom", boom), ("soft_version", soft)):
        if not SAFE_VALUE.fullmatch(item):
            raise SkillError("Error_InvalidInput", f"{label} has unsupported characters: {item!r}")

    if not need_hubs_sdk(values):
        raise SkillError(
            "Error_HubsNotNeeded",
            f"all {list(NEED_HUBS_LIST_KEYS)} are empty; this product builds no prebuilt HUBS artefacts",
        )

    workspace = Path(args.workspace).resolve() if args.workspace else default_workspace(env_file)
    platform = args.platform or default_platform(env_file)
    if not platform:
        raise SkillError(
            "Error_PlatformMissing",
            f"could not infer platform from {env_file}; pass --platform explicitly",
        )

    cache_root = Path(args.hubs_cache_dir).resolve() if args.hubs_cache_dir else workspace / DEFAULT_HUBS_CACHE_SUBDIR
    downloads_dir = cache_root / "downloads"
    staging = cache_root / f"staging_{args.product_name or env_file.parent.name}"
    downloads_dir.mkdir(parents=True, exist_ok=True)
    cache_root.mkdir(parents=True, exist_ok=True)

    pinned = args.soft_version is not None
    pin_value = args.soft_version or soft
    package_name, url = build_urls(args.base_url, product, pcb, boom, pin_value, pinned)
    archive = downloads_dir / package_name

    mpu_target = Path(args.mpu_target).resolve() if args.mpu_target else workspace / platform / "sdk"
    mcu_target = Path(args.mcu_target).resolve() if args.mcu_target else workspace / platform / "mcu" / "sdk"

    if not args.force:
        for target, label in ((mpu_target, "mpu"), (mcu_target, "mcu")):
            if target.exists():
                raise SkillError("Error_SdkExists", f"{label} target already exists: {target}; pass --force to replace")

    credential, cred_source = resolve_credential(args.cred, values, workspace)

    download(url, credential, archive)
    extracted = unpack(archive, staging)
    place_sdk(extracted, f"sdk/{args.build_model}", mpu_target)
    place_sdk(extracted, f"sdk/{args.mcu_build_model}", mcu_target)

    return {
        "list_env": str(env_file),
        "workspace": str(workspace),
        "platform": platform,
        "cache_root": str(cache_root),
        "downloads_dir": str(downloads_dir),
        "staging_dir": str(staging),
        "archive": str(archive),
        "product": product,
        "pcb_version": pcb,
        "boom_version": boom,
        "soft_version": soft,
        "package_mode": "pinned" if pinned else "latest",
        "package_name": package_name,
        "download_url": url,
        "credential_source": cred_source,
        "mpu_build_model": args.build_model,
        "mcu_build_model": args.mcu_build_model,
        "mpu_target": str(mpu_target),
        "mcu_target": str(mcu_target),
        "mpu_sdk_layout": expected_layouts(mpu_target),
        "mcu_sdk_layout": expected_layouts(mcu_target),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Download and unpack the HUBS prebuilt SDK.")
    parser.add_argument("--list-env", required=True, help="path to the product config/<product>/list.env")
    parser.add_argument("--workspace", help="workspace root; defaults to the list.env owner's workspace")
    parser.add_argument("--platform", help="platform variant (e.g. s32k312_asr1803); inferred from list.env path")
    parser.add_argument("--hubs-cache-dir", help="HUBS cache root; default <workspace>/.ai_tmp/hubs-sdk")
    parser.add_argument("--build-model", default=DEFAULT_BUILD_MODEL, help="MPU build model directory inside the package")
    parser.add_argument("--mpu-target", help="MPU SDK target dir; default <workspace>/<platform>/sdk")
    parser.add_argument("--mcu-build-model", default=DEFAULT_MCU_BUILD_MODEL, help="MCU build model directory inside the package")
    parser.add_argument("--mcu-target", help="MCU SDK target dir; default <workspace>/<platform>/mcu/sdk")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL, help="Artifactory project base URL")
    parser.add_argument("--soft-version", help="pin this soft version instead of the latest package")
    parser.add_argument("--product-name", help="staging folder suffix; defaults to the config directory name")
    parser.add_argument("--cred", help="Artifactory credential user:password (used only as a last resort)")
    parser.add_argument("--force", action="store_true", help="replace existing MPU/MCU SDK targets")
    args = parser.parse_args(argv)

    try:
        data = run(args)
    except SkillError as error:
        emit(response("error", code=error.code, message=str(error)))
        return 1
    except (OSError, zipfile.BadZipFile) as error:
        emit(response("error", code="Error_IO", message=str(error)))
        return 2
    emit(response("ok", data=data))
    return 0


if __name__ == "__main__":
    sys.exit(main())