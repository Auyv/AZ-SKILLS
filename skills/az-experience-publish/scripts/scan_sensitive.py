#!/usr/bin/env python3
import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path

PATTERNS = {
    "email": re.compile(r"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b"),
    "private_ipv4": re.compile(r"\b(?:10\.\d{1,3}\.\d{1,3}\.\d{1,3}|192\.168\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})\b"),
    "windows_absolute_path": re.compile(r"\b[A-Za-z]:\\[^\r\n`]+"),
    "linux_user_path": re.compile(r"/(?:home|root)/[^\s`]+"),
    "long_hex_identifier": re.compile(r"\b[0-9a-fA-F]{32,}\b"),
    "secret_assignment": re.compile(r"(?i)\b(?:token|password|passwd|secret|api[_-]?key)\s*[:=]\s*[^\s]+"),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("file")
    parser.add_argument("--term", action="append", default=[])
    args = parser.parse_args()
    timestamp = datetime.now(timezone.utc).isoformat()
    path = Path(args.file)
    if not path.is_file():
        print(json.dumps({"status": "error", "timestamp": timestamp, "error": {"code": "ERR_EXPERIENCE_NOT_FOUND", "message": str(path)}}, ensure_ascii=False, indent=2))
        return 2
    text = path.read_text(encoding="utf-8")
    findings = []
    for name, pattern in PATTERNS.items():
        for match in pattern.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            value = match.group(0)
            findings.append({"type": name, "line": line, "sample": value[:120]})
    for term in args.term:
        if not term:
            continue
        for match in re.finditer(re.escape(term), text, re.I):
            findings.append({"type": "custom_term", "line": text.count("\n", 0, match.start()) + 1, "sample": term})
    status = "error" if findings else "success"
    result = {"status": status, "timestamp": timestamp, "data": {"file": str(path.resolve()), "findings": findings}}
    if findings:
        result["error"] = {"code": "ERR_SENSITIVE_CONTENT", "message": f"发现 {len(findings)} 个待审核项"}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 2 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
