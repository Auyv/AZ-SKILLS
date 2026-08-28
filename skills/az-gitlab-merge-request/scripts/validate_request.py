#!/usr/bin/env python3
"""Validate a source/target GitLab merge-request pair without network access."""
from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone


def result(status: str, data: dict | None = None, code: str | None = None, message: str | None = None) -> dict:
    output = {"status": status, "timestamp": datetime.now(timezone.utc).isoformat()}
    if data is not None:
        output["data"] = data
    if code is not None:
        output["error"] = {"code": code, "message": message}
    return output


def validate(source_branch: str, target_branch: str) -> dict:
    if not source_branch or not target_branch or "\n" in source_branch or "\n" in target_branch:
        return result("error", code="ERR_INVALID_BRANCH", message="源分支和目标分支必须非空且不能包含换行符。")
    if source_branch == target_branch:
        return result("error", code="ERR_INVALID_BRANCH", message="源分支与目标分支必须不同。")
    return result("success", data={"source_branch": source_branch, "target_branch": target_branch})


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_branch")
    parser.add_argument("target_branch")
    args = parser.parse_args()
    output = validate(args.source_branch, args.target_branch)
    print(json.dumps(output, ensure_ascii=False))
    return 0 if output["status"] == "success" else 2


if __name__ == "__main__":
    raise SystemExit(main())
