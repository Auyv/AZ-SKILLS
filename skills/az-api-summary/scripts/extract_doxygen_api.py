from __future__ import annotations

import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

try:
    from lxml import html
except ImportError:
    print(json.dumps({
        "status": "error",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "error": {
            "code": "ERR_DEPENDENCY_MISSING",
            "message": "缺少 lxml；请使用已有 HTML 解析工具或人工读取 Doxygen 页面，不要自动安装",
        },
    }, ensure_ascii=False, indent=2))
    raise SystemExit(1)


NAME_PATTERN = re.compile(
    r"\b(_?(?:api|apdu|tpdu|proto|hal|port|peripheral|util)_[A-Za-z0-9_]+)\b"
)


def clean(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


def node_text(node) -> str:
    return clean(" ".join(node.itertext()))


def parse_page(path: Path) -> dict:
    tree = html.fromstring(path.read_bytes().decode("utf-8", errors="replace"))
    title_nodes = tree.xpath(
        "//div[contains(concat(' ',normalize-space(@class),' '),' headertitle ')]"
        "/div[contains(concat(' ',normalize-space(@class),' '),' title ')]"
    )
    parent_nodes = tree.xpath(
        "//div[contains(concat(' ',normalize-space(@class),' '),' ingroups ')]"
    )
    title = node_text(title_nodes[0]) if title_nodes else path.stem
    parent = node_text(parent_nodes[0]) if parent_nodes else ""
    functions = {}

    rows = tree.xpath(
        "//table[contains(concat(' ',normalize-space(@class),' '),' memberdecls ')]"
        "/tr[starts-with(@class,'memitem')]"
    )
    for row in rows:
        right = row.xpath(
            "./td[contains(concat(' ',normalize-space(@class),' '),' memItemRight ')]"
        )
        if not right:
            continue
        match = NAME_PATTERN.search(node_text(right[0]))
        if not match:
            continue
        name = match.group(1)
        row_id = row.get("class", "").replace("memitem:", "")
        desc = tree.xpath(
            f"//tr[contains(@class,'memdesc:{row_id}')]"
            "//td[contains(concat(' ',normalize-space(@class),' '),' mdescRight ')]"
        ) if row_id else []
        functions[name] = {
            "name": name,
            "prototype": node_text(row),
            "brief": node_text(desc[0]) if desc else "",
        }

    return {
        "file": path.name,
        "title": title,
        "parent": parent,
        "functions": list(functions.values()),
    }


def main() -> None:
    if len(sys.argv) != 3:
        print("用法: extract_doxygen_api.py <html_root> <output.json>", file=sys.stderr)
        raise SystemExit(2)
    root = Path(sys.argv[1])
    output = Path(sys.argv[2])
    if not root.is_dir():
        print(json.dumps({
            "status": "error",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "error": {"code": "ERR_FILE_NOT_FOUND", "message": f"HTML 目录不存在: {root}"},
        }, ensure_ascii=False, indent=2))
        raise SystemExit(1)

    candidates = sorted(set(root.glob("group_*.html")) | set(root.glob("*_8h.html")))
    pages = [parse_page(path) for path in candidates]
    pages = [page for page in pages if page["functions"]]
    unique = sorted({f["name"] for page in pages for f in page["functions"]})
    if not unique:
        result = {
            "status": "error",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "error": {"code": "ERR_API_EXTRACTION_EMPTY", "message": "未识别到 API/协议/移植函数"},
        }
        print(json.dumps(result, ensure_ascii=False, indent=2))
        raise SystemExit(1)

    manifest = {
        "status": "success",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "data": {
            "source_root": str(root.resolve()),
            "page_count": len(pages),
            "unique_function_count": len(unique),
            "unique_functions": unique,
            "pages": pages,
        },
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({
        "status": "success",
        "timestamp": manifest["timestamp"],
        "data": {
            "output": str(output.resolve()),
            "page_count": len(pages),
            "unique_function_count": len(unique),
        },
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

