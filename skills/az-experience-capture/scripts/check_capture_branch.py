#!/usr/bin/env python3
import argparse
import json
import re
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


def git(repo, *args):
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--prefix", default="experience/")
    parser.add_argument("--base-branch", default="master")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    probe = git(repo, "rev-parse", "--is-inside-work-tree")
    if probe.returncode != 0 or probe.stdout.strip() != "true":
        emit("error", code="ERR_REPO_NOT_FOUND", message=str(repo))
        return 2

    current = git(repo, "branch", "--show-current")
    branch = current.stdout.strip()
    if current.returncode != 0 or not branch:
        emit("error", code="ERR_INVALID_WORK_BRANCH", message="当前不是命名分支")
        return 2

    if branch in {args.base_branch, "main", "master"}:
        emit(
            "error",
            code="ERR_PROTECTED_BRANCH",
            message=f"禁止直接在 {branch} 编写经验，请创建 {args.prefix}<slug>",
        )
        return 2

    prefix = args.prefix.rstrip("/") + "/"
    slug = branch[len(prefix):] if branch.startswith(prefix) else ""
    if not slug or not re.fullmatch(r"[a-z0-9][a-z0-9._-]*", slug):
        emit(
            "error",
            code="ERR_INVALID_WORK_BRANCH",
            message=f"分支必须符合 {prefix}<lowercase-slug>",
        )
        return 2

    emit(
        "success",
        data={
            "repo": str(repo),
            "work_branch": branch,
            "base_branch": args.base_branch,
            "merge_request_status": "not_created",
            "publish_eligible": False,
        },
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
