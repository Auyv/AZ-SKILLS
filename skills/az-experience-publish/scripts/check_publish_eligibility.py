#!/usr/bin/env python3
import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def emit(status, data=None, code=None, message=None):
    result = {
        "status": status,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }
    if data is not None:
        result["data"] = data
    if code is not None:
        result["error"] = {"code": code, "message": message}
    print(json.dumps(result, ensure_ascii=False, indent=2))


def git(repo, *args, text=True):
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        check=False,
        capture_output=True,
        text=text,
        encoding="utf-8" if text else None,
        errors="replace" if text else None,
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--experience", required=True)
    parser.add_argument("--base-ref", default="origin/master")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    probe = git(repo, "rev-parse", "--is-inside-work-tree")
    if probe.returncode != 0 or probe.stdout.strip() != "true":
        emit("error", code="ERR_REPO_NOT_FOUND", message=str(repo))
        return 2

    source = Path(args.experience)
    if not source.is_absolute():
        source = repo / source
    source = source.resolve()
    try:
        relative = source.relative_to(repo).as_posix()
    except ValueError:
        emit("error", code="ERR_EXPERIENCE_NOT_FOUND", message="经验文件必须位于仓库内")
        return 2
    if not relative.startswith("experiences/") or source.suffix.lower() != ".md":
        emit("error", code="ERR_EXPERIENCE_NOT_FOUND", message="仅允许发布 experiences/ 下的 Markdown 经验")
        return 2
    if not source.is_file():
        emit("error", code="ERR_EXPERIENCE_NOT_FOUND", message=str(source))
        return 2

    base = git(repo, "rev-parse", "--verify", f"{args.base_ref}^{{commit}}")
    if base.returncode != 0:
        emit(
            "error",
            code="ERR_BASE_REF_NOT_FOUND",
            message=f"找不到 {args.base_ref}，请先 git fetch origin master",
        )
        return 2

    merged = git(repo, "cat-file", "-e", f"{args.base_ref}:{relative}")
    if merged.returncode != 0:
        emit(
            "error",
            code="ERR_NOT_MERGED_TO_MASTER",
            message=f"{relative} 不存在于 {args.base_ref}",
        )
        return 2

    remote_content = git(repo, "show", f"{args.base_ref}:{relative}", text=False)
    if remote_content.returncode != 0:
        emit("error", code="ERR_NOT_MERGED_TO_MASTER", message=remote_content.stderr.decode("utf-8", "replace"))
        return 2
    local_content = source.read_bytes()
    if local_content != remote_content.stdout:
        emit(
            "error",
            data={
                "experience": relative,
                "base_ref": args.base_ref,
                "local_sha256": hashlib.sha256(local_content).hexdigest(),
                "base_sha256": hashlib.sha256(remote_content.stdout).hexdigest(),
            },
            code="ERR_SOURCE_DIVERGED",
            message="本地来源经验与已合并版本不一致",
        )
        return 2

    blob = git(repo, "rev-parse", f"{args.base_ref}:{relative}")
    emit(
        "success",
        data={
            "experience": relative,
            "base_ref": args.base_ref,
            "base_commit": base.stdout.strip(),
            "blob": blob.stdout.strip(),
            "sha256": hashlib.sha256(local_content).hexdigest(),
            "merge_verified": True,
            "publish_eligible": True,
        },
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
