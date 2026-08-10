#!/usr/bin/env python3
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

REQUIRED_FIELDS = ["title", "status", "visibility", "tags", "symptoms", "environment", "verified_at"]
REQUIRED_SECTIONS = ["现象", "根因", "排查思路", "解决方法", "验证与回退", "适用边界"]


def output(status, data=None, code=None, message=None):
    result = {"status": status, "timestamp": datetime.now(timezone.utc).isoformat()}
    if data is not None:
        result["data"] = data
    if code:
        result["error"] = {"code": code, "message": message}
    print(json.dumps(result, ensure_ascii=False, indent=2))


def main():
    if len(sys.argv) != 2:
        output("error", code="ERR_EMPTY_CONTEXT", message="用法: validate_experience.py <experience.md>")
        return 2
    path = Path(sys.argv[1])
    if not path.is_file():
        output("error", code="ERR_REPO_NOT_FOUND", message=f"文件不存在: {path}")
        return 2
    text = path.read_text(encoding="utf-8")
    match = re.match(r"^---\s*\n(.*?)\n---\s*\n", text, re.S)
    if not match:
        output("error", code="ERR_EMPTY_CONTEXT", message="缺少 YAML frontmatter")
        return 2
    frontmatter = match.group(1)
    missing_fields = [f for f in REQUIRED_FIELDS if not re.search(rf"(?m)^{re.escape(f)}\s*:", frontmatter)]
    missing_sections = [s for s in REQUIRED_SECTIONS if not re.search(rf"(?m)^##\s+{re.escape(s)}\s*$", text)]
    if missing_fields or missing_sections:
        output("error", code="ERR_EMPTY_CONTEXT", message="经验结构不完整", data={"missing_fields": missing_fields, "missing_sections": missing_sections})
        return 2
    output("success", {"file": str(path.resolve()), "required_fields": len(REQUIRED_FIELDS), "required_sections": len(REQUIRED_SECTIONS)})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
