#!/usr/bin/env python3
import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path


def terms(text):
    text = text.lower()
    found = re.findall(r"[a-z0-9_.:+-]{2,}|[\u4e00-\u9fff]{2,}", text)
    result = set(found)
    for item in found:
        if re.fullmatch(r"[\u4e00-\u9fff]+", item) and len(item) > 2:
            result.update(item[i:i + 2] for i in range(len(item) - 1))
    return sorted(result, key=len, reverse=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--query", default="")
    parser.add_argument("--reference", default="")
    parser.add_argument("--top-k", type=int, default=5)
    args = parser.parse_args()
    timestamp = datetime.now(timezone.utc).isoformat()
    repo = Path(args.repo)
    root = repo / "experiences"
    if not root.is_dir():
        print(json.dumps({"status": "error", "timestamp": timestamp, "error": {"code": "ERR_REPO_NOT_FOUND", "message": str(root)}}, ensure_ascii=False, indent=2))
        return 2
    if not args.query.strip():
        print(json.dumps({"status": "error", "timestamp": timestamp, "error": {"code": "ERR_EMPTY_QUERY", "message": "query 不能为空"}}, ensure_ascii=False, indent=2))
        return 2
    if len(args.query) > 20000:
        print(json.dumps({"status": "error", "timestamp": timestamp, "error": {"code": "ERR_QUERY_TOO_LONG", "message": "query 超过 20000 字符"}}, ensure_ascii=False, indent=2))
        return 2
    query_text = args.query
    if args.reference:
        ref = Path(args.reference)
        if not ref.is_absolute():
            ref = repo / ref
        if not ref.is_file():
            print(json.dumps({"status": "error", "timestamp": timestamp, "error": {"code": "ERR_REFERENCE_NOT_FOUND", "message": str(ref)}}, ensure_ascii=False, indent=2))
            return 2
        query_text += "\n" + ref.read_text(encoding="utf-8")[:10000]
    query_terms = terms(query_text)
    matches = []
    for path in root.rglob("*.md"):
        text = path.read_text(encoding="utf-8", errors="replace")
        lowered = text.lower()
        title = path.stem.lower()
        matched = [term for term in query_terms if term in lowered]
        if not matched:
            continue
        score = sum(lowered.count(term) for term in matched) + sum(3 for term in matched if term in title)
        matches.append({"path": str(path.relative_to(repo)).replace("\\", "/"), "score": score, "matched_terms": matched[:12]})
    matches.sort(key=lambda item: (-item["score"], item["path"]))
    matches = matches[:max(1, min(args.top_k, 20))]
    status = "success" if matches else "partial"
    data = {"query_terms": query_terms[:30], "matches": matches}
    if not matches:
        data["warning"] = {"code": "ERR_NO_MATCH", "message": "未找到相似经验"}
    print(json.dumps({"status": status, "timestamp": timestamp, "data": data}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
