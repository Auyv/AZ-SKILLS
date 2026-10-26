#!/usr/bin/env python3
"""Prepare CTP manifest repositories and download the HUBS SDK.

Runtime dependencies: Python 3 standard library and the native ``git`` CLI.
No curl, unzip, PyYAML, or external manifest-pull helper is used.
All outputs are JSON so callers can retain per-project results and diagnostics.
"""
from __future__ import annotations

import argparse
import base64
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path, PurePosixPath

DEFAULT_BASE_URL = "http://172.16.1.99:8081/artifactory/sei/project"
HUBS_KEYS = (
    "G_PROJ_HUBS_PRODUCT_NAME", "G_PROJ_HUBS_PCB_VERSION",
    "G_PROJ_HUBS_BOOM_VERSION", "G_PROJ_HUBS_SOFT_VERSION",
)
LIST_KEYS = (
    "G_PROJ_LIST_HUBS_LIB", "G_PROJ_LIST_HUBS_SO", "G_PROJ_LIST_HUBS_PLUG",
    "G_PROJ_LIST_HUBS_BIN", "G_PROJ_LIST_HUBS_DRV",
)
CRED_KEYS = (
    "G_PROJ_HUBS_ARTIFACTORY_CRED", "G_PROJ_HUBS_SDK_CRED",
    "G_PROJ_ARTIFACTORY_CRED", "G_HUBS_ARTIFACTORY_CRED",
)
EXPORT = re.compile(r'^\s*export\s+([A-Za-z_][A-Za-z0-9_]*)=(.*)\s*$')
SAFE = re.compile(r"^[A-Za-z0-9._-]+$")


class WorkReadyError(Exception):
    pass


def emit(data: dict, code: int = 0) -> int:
    print(json.dumps(data, ensure_ascii=False, indent=2))
    return code


def git(args: list[str], cwd: Path | None = None, check: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(["git", *args], cwd=cwd, text=True, capture_output=True)
    if check and result.returncode:
        detail = (result.stderr or result.stdout).strip()
        raise WorkReadyError(f"git {' '.join(args[:3])} failed ({result.returncode}): {detail}")
    return result


def parse_env(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise WorkReadyError(f"list.env not found: {path}")
    text = path.read_text(encoding="utf-8-sig", errors="replace")
    values: dict[str, str] = {}
    for line in text.splitlines():
        match = EXPORT.match(line)
        if not match:
            continue
        value = match.group(2).strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[match.group(1)] = value.strip()
    return values


def parse_manifest(path: Path) -> list[dict]:
    root = ET.parse(path).getroot()
    default = root.find("default")
    default_remote = default.get("remote", "") if default is not None else ""
    default_revision = default.get("revision", "") if default is not None else ""
    remotes = {node.get("name", ""): (node.get("fetch", ""), node.get("revision", ""))
               for node in root.findall("remote")}
    projects = []
    for node in root.findall("project"):
        name, relpath = node.get("name", ""), node.get("path") or node.get("name", "")
        remote = node.get("remote", default_remote)
        remote_fetch, remote_revision = remotes.get(remote, ("", ""))
        revision = node.get("revision") or remote_revision or default_revision
        fetch = remote_fetch
        if not name or not relpath or not fetch:
            raise WorkReadyError(f"invalid project entry in {path}: name/path/remote fetch missing")
        project_path = PurePosixPath(relpath)
        if project_path.is_absolute() or ".." in project_path.parts or "\\" in relpath:
            raise WorkReadyError(f"unsafe manifest path: {relpath}")
        if re.match(r"^(https?|ssh|git)://|^git@", name):
            url = name
        elif re.fullmatch(r"git@[^:]+:", fetch):
            url = fetch + name.lstrip("/")
        else:
            url = fetch.rstrip("/") + "/" + name.lstrip("/")
        projects.append({"name": name, "path": relpath, "remote": remote,
                         "revision": revision, "url": url, "manifest": str(path)})
    return projects


def ignored(project: dict, ignores: set[tuple[str, str]]) -> bool:
    return (project["remote"], project["name"]) in ignores


def project_key(project: dict) -> tuple[str, str]:
    return project["remote"], project["name"]


def run_pull(args: argparse.Namespace) -> tuple[dict, int]:
    workspace = Path(args.workspace).resolve()
    ignores = set()
    for item in args.ignore:
        remote, sep, name = item.partition("|")
        if not sep or not remote or not name:
            raise WorkReadyError("--ignore format must be REMOTE|PROJECT_NAME")
        ignores.add((remote, name))

    unique: dict[tuple[str, str], dict] = {}
    for manifest_arg in args.manifest:
        for project in parse_manifest(Path(manifest_arg)):
            key = project_key(project)
            pull_key = (*key, project["path"])
            if pull_key not in unique:
                unique[pull_key] = project
            else:
                # Keep repeated manifest references attached to the same identity/path row.
                unique[pull_key].setdefault("manifest_refs", [unique[pull_key]["manifest"]]).append(project["manifest"])
    ordered = sorted(unique.values(), key=lambda p: (len(PurePosixPath(p["path"]).parts), p["path"].casefold()))
    results = []
    for project in ordered:
        row = {k: project.get(k) for k in ("path", "name", "remote", "revision", "manifest")}
        if project.get("manifest_refs"):
            row["manifest_refs"] = project["manifest_refs"]
        target = (workspace / Path(*PurePosixPath(project["path"]).parts)).resolve()
        if not target.is_relative_to(workspace):
            row.update(status="failed", error="resolved target escapes workspace")
            results.append(row)
            continue
        if ignored(project, ignores):
            row["status"] = "ignored"
            results.append(row)
            continue
        try:
            if target.exists():
                if not (target / ".git").exists():
                    raise WorkReadyError(f"target exists but is not a Git repository: {target}")
                origin = git(["-C", str(target), "remote", "get-url", "origin"]).stdout.strip()
                if origin.rstrip("/").removesuffix(".git") != project["url"].rstrip("/").removesuffix(".git"):
                    raise WorkReadyError(f"origin mismatch at {target}: {origin}")
                dirty = git(["-C", str(target), "status", "--porcelain"]).stdout.strip()
                if dirty:
                    raise WorkReadyError(f"repository has local changes: {target}")
                git(["-C", str(target), "fetch", "--prune", "origin"])
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                git(["clone", "--no-checkout", project["url"], str(target)])
            revision = project["revision"]
            if not revision or revision.startswith("-"):
                raise WorkReadyError("manifest project has no resolved revision")
            if re.fullmatch(r"[0-9a-fA-F]{40,64}", revision):
                if target.exists() and (target / ".git").exists():
                    git(["-C", str(target), "fetch", "origin", revision], check=False)
                git(["-C", str(target), "checkout", "--detach", revision])
            else:
                local_branch = git(["-C", str(target), "show-ref", "--verify", "--quiet",
                                    f"refs/heads/{revision}"], check=False).returncode == 0
                if local_branch:
                    git(["-C", str(target), "checkout", revision])
                    git(["-C", str(target), "pull", "--ff-only", "origin", revision])
                else:
                    remote_branch = git(["-C", str(target), "show-ref", "--verify", "--quiet",
                                         f"refs/remotes/origin/{revision}"], check=False).returncode == 0
                    if remote_branch:
                        git(["-C", str(target), "checkout", "--track", "-b", revision, f"origin/{revision}"])
                    else:
                        git(["-C", str(target), "checkout", "--detach", revision])
            row.update(status="success", head=git(["-C", str(target), "rev-parse", "HEAD"]).stdout.strip(),
                       branch=git(["-C", str(target), "branch", "--show-current"]).stdout.strip())
        except (WorkReadyError, OSError) as exc:
            row.update(status="failed", error=str(exc))
        results.append(row)
    counts = {state: sum(1 for item in results if item["status"] == state)
              for state in ("success", "failed", "ignored")}
    counts["distinct_repositories"] = len({(row["remote"], row["name"]) for row in results})
    return {"workspace": str(workspace), "execution": "sequential-parent-first", "counts": counts,
            "projects": results}, (1 if counts["failed"] else 0)


def credential(args: argparse.Namespace, values: dict, build_script: Path | None) -> tuple[str, str]:
    for key in CRED_KEYS:
        if values.get(key, "").strip():
            return values[key].strip(), f"list.env:{key}"
    if os.environ.get("HUBS_ARTIFACTORY_CRED", "").strip():
        return os.environ["HUBS_ARTIFACTORY_CRED"].strip(), "env:HUBS_ARTIFACTORY_CRED"
    if build_script and build_script.is_file():
        content = build_script.read_text(encoding="utf-8", errors="replace")
        for pattern in (r"curl\s+[^\n]*?-u\s*['\"]?([^\s'\"]+)", r"curl\s+[^\n]*?-u([^\s]+)"):
            found = re.search(pattern, content)
            if found and "$" not in found.group(1):
                return found.group(1), f"build-script:{build_script.name}"
    if args.cred:
        return args.cred, "--cred"
    raise WorkReadyError("Error_CredMissing: no credential in list.env, environment, matched build script, or --cred")


def safe_extract(archive: Path, staging: Path) -> None:
    staging.mkdir(parents=True, exist_ok=True)
    root = staging.resolve()
    with zipfile.ZipFile(archive) as zf:
        for info in zf.infolist():
            member = PurePosixPath(info.filename)
            if member.is_absolute() or ".." in member.parts:
                raise WorkReadyError(f"unsafe ZIP entry: {info.filename}")
            mode = info.external_attr >> 16
            if mode and (mode & 0o170000) == 0o120000:
                raise WorkReadyError(f"ZIP symlink is not allowed: {info.filename}")
            destination = (staging / Path(*member.parts)).resolve()
            if not destination.is_relative_to(root):
                raise WorkReadyError(f"ZIP entry escapes staging: {info.filename}")
            if info.is_dir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info) as src, destination.open("xb") as dst:
                shutil.copyfileobj(src, dst)


def download_file(url: str, cred: str, archive: Path) -> None:
    if archive.exists():
        if archive.stat().st_size == 0:
            raise WorkReadyError(f"cached archive is empty: {archive}; preserve it and resolve manually")
        return
    token = base64.b64encode(cred.encode("utf-8")).decode("ascii")
    request = urllib.request.Request(url, headers={"Authorization": f"Basic {token}"})
    temp = archive.with_suffix(archive.suffix + ".part")
    if temp.exists():
        raise WorkReadyError(f"partial download already exists; preserving it: {temp}")
    try:
        with urllib.request.urlopen(request, timeout=120) as response, temp.open("wb") as output:
            shutil.copyfileobj(response, output)
        if temp.stat().st_size == 0:
            raise WorkReadyError("downloaded archive is empty")
        temp.replace(archive)
    except (urllib.error.URLError, OSError) as exc:
        raise WorkReadyError(f"Error_Download: {exc}") from exc
    finally:
        if temp.exists():
            temp.unlink()


def run_sdk(args: argparse.Namespace) -> dict:
    env_file = Path(args.list_env).resolve()
    values = parse_env(env_file)
    missing = [key for key in HUBS_KEYS if not values.get(key, "").strip()]
    if missing:
        raise WorkReadyError(f"list.env lacks required HUBS keys: {missing}")
    if not any(values.get(key, "").strip() for key in LIST_KEYS):
        raise WorkReadyError("HUBS SDK not required: all G_PROJ_LIST_HUBS_* fields are empty")
    workspace = Path(args.workspace).resolve()
    platform = args.platform
    if not platform:
        parts = env_file.parts
        try:
            platform = parts[parts.index(workspace.name) + 1]
        except (ValueError, IndexError):
            raise WorkReadyError("cannot infer platform; pass --platform")
    model = args.build_model or values.get("G_PRODUCT_BUILD_MPU_MODEL") or "ag35_ceraa"
    mcu_model = args.mcu_build_model or values.get("G_PRODUCT_BUILD_MCU_MODEL") or "s32k312"
    profile = args.profile or env_file.parent.name
    product, pcb, boom, soft = (values[k] for k in HUBS_KEYS)
    for label, value in (("product", values[HUBS_KEYS[0]]), ("PCB", values[HUBS_KEYS[1]]), ("BOOM", values[HUBS_KEYS[2]])):
        if not SAFE.fullmatch(value):
            raise WorkReadyError(f"invalid {label} value in list.env")
    for label, value in (("soft version", soft), ("MPU build model", model),
                         ("MCU build model", mcu_model), ("profile", profile)):
        if not SAFE.fullmatch(value):
            raise WorkReadyError(f"invalid {label}")
    base = args.base_url.rstrip("/")
    pinned = bool(args.soft_version)
    package = f"{product}_{pcb}{boom}{('_' + args.soft_version) if pinned else ''}.zip"
    url = f"{base}/{product}/{pcb}{boom}/{'history/' if pinned else ''}{package}"
    cache = Path(args.cache_dir).resolve() if args.cache_dir else workspace / ".ai_tmp" / "hubs-sdk"
    downloads = cache / "downloads"
    stage = cache / f"staging_{profile}"
    archive = downloads / package
    mpu = Path(args.mpu_target).resolve() if args.mpu_target else workspace / platform / "sdk"
    mcu = Path(args.mcu_target).resolve() if args.mcu_target else workspace / platform / "mcu" / "sdk"
    if mpu.exists() or mcu.exists():
        raise WorkReadyError(f"SDK target exists; refusing overwrite: MPU={mpu.exists()}, MCU={mcu.exists()}")
    config_repo = args.config_repo
    build_script = workspace / platform / config_repo / "build" / "mpu" / f"build_{model}.sh" if config_repo else None
    cred, cred_source = credential(args, values, build_script)
    downloads.mkdir(parents=True, exist_ok=True)
    download_file(url, cred, archive)
    if stage.exists():
        if not (stage / "sdk").is_dir():
            raise WorkReadyError(f"staging exists without sdk/; preserving it: {stage}")
    else:
        safe_extract(archive, stage)
    mpu_source, mcu_source = stage / "sdk" / model, stage / "sdk" / mcu_model
    if not mpu_source.is_dir() or not mcu_source.is_dir():
        raise WorkReadyError(f"Error_BuildModel: missing source model(s): MPU={mpu_source.is_dir()}, MCU={mcu_source.is_dir()}")
    mpu.parent.mkdir(parents=True, exist_ok=True)
    mcu.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(mpu_source, mpu)
    shutil.copytree(mcu_source, mcu)
    return {"status": "ok", "workspace": str(workspace), "platform": platform,
            "package_name": package, "package_mode": "pinned" if pinned else "latest",
            "download_url": url, "credential_source": cred_source, "archive": str(archive),
            "staging": str(stage), "mpu_model": model, "mcu_model": mcu_model,
            "mpu_target": str(mpu), "mcu_target": str(mcu),
            "mpu_layout": sorted(p.name for p in mpu.iterdir()),
            "mcu_layout": sorted(p.name for p in mcu.iterdir())}


def run_baseline(args: argparse.Namespace) -> dict:
    workspace = Path(args.workspace).resolve()
    ignores = set()
    for item in args.ignore:
        remote, sep, name = item.partition("|")
        if not sep:
            raise WorkReadyError("--ignore format must be REMOTE|PROJECT_NAME")
        ignores.add((remote, name))
    unique: dict[tuple[str, str], dict] = {}
    for manifest in args.manifest:
        for project in parse_manifest(Path(manifest)):
            unique.setdefault((*project_key(project), project["path"]), project)
    rows = []
    for project in unique.values():
        target = workspace / Path(*PurePosixPath(project["path"]).parts)
        row = {k: project[k] for k in ("path", "name", "remote", "revision")}
        if ignored(project, ignores):
            row["status"] = "ignored"
        elif not (target / ".git").exists():
            row["status"] = "missing"
        else:
            head = git(["-C", str(target), "rev-parse", "HEAD"], check=False)
            branch = git(["-C", str(target), "branch", "--show-current"], check=False)
            status = git(["-C", str(target), "status", "--short"], check=False)
            row.update(status="present" if head.returncode == 0 else "error",
                       head=head.stdout.strip(), branch=branch.stdout.strip(),
                       dirty=status.stdout.splitlines())
        rows.append(row)
    summary = {key: sum(row["status"] == key for row in rows)
               for key in ("present", "missing", "ignored", "error")}
    return {"workspace": str(workspace), "counts": summary, "projects": rows}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="CTP work-ready helper (Python stdlib + Git CLI).")
    sub = parser.add_subparsers(dest="command", required=True)
    pull = sub.add_parser("pull", help="pull manifest projects sequentially, parent first")
    pull.add_argument("--workspace", required=True)
    pull.add_argument("--manifest", action="append", required=True, help="XML manifest; repeat for MPU and MCU")
    pull.add_argument("--ignore", action="append", default=[], help="exact identity REMOTE|PROJECT_NAME")
    sdk = sub.add_parser("download-sdk", help="download/unpack HUBS SDK without curl or unzip")
    sdk.add_argument("--list-env", required=True)
    sdk.add_argument("--workspace", required=True)
    sdk.add_argument("--platform", required=True)
    sdk.add_argument("--config-repo", help="repo directory name under <workspace>/<platform>")
    sdk.add_argument("--profile", "--product-name", dest="profile")
    sdk.add_argument("--build-model")
    sdk.add_argument("--mcu-build-model")
    sdk.add_argument("--base-url", default=DEFAULT_BASE_URL)
    sdk.add_argument("--soft-version", help="select history package; omit for latest")
    sdk.add_argument("--cache-dir", "--hubs-cache-dir", dest="cache_dir")
    sdk.add_argument("--mpu-target")
    sdk.add_argument("--mcu-target")
    sdk.add_argument("--cred", help="last resort user:password; never printed")
    baseline = sub.add_parser("baseline", help="inventory manifest repositories read-only")
    baseline.add_argument("--workspace", required=True)
    baseline.add_argument("--manifest", action="append", required=True)
    baseline.add_argument("--ignore", action="append", default=[])
    args = parser.parse_args(argv)
    try:
        if args.command == "pull":
            payload, code = run_pull(args)
            return emit(payload, code)
        if args.command == "download-sdk":
            return emit(run_sdk(args))
        return emit(run_baseline(args))
    except (WorkReadyError, OSError, ET.ParseError, zipfile.BadZipFile) as exc:
        return emit({"status": "error", "error": str(exc)}, 1)


if __name__ == "__main__":
    sys.exit(main())
