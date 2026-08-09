from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path


SUPPORTED = {".chm", ".pdf", ".html", ".htm", ".md", ".docx"}


def response(status: str, data=None, code=None, message=None):
    result = {
        "status": status,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }
    if data is not None:
        result["data"] = data
    if code is not None:
        result["error"] = {"code": code, "message": message}
    return result


def fail(code: str, message: str) -> None:
    print(json.dumps(response("error", code=code, message=message), ensure_ascii=False, indent=2))
    raise SystemExit(1)


def validate(payload: object) -> dict:
    if not isinstance(payload, dict):
        fail("ERR_TYPE_INVALID", "输入根对象必须是 JSON object")
    sources = payload.get("source_documents")
    if sources is None or sources == []:
        fail("ERR_MISSING_INPUT", "缺少必需字段 source_documents")
    if not isinstance(sources, list) or not all(isinstance(item, str) for item in sources):
        fail("ERR_TYPE_INVALID", "source_documents 必须是非空字符串数组")

    resolved = []
    for raw in sources:
        path = Path(raw).expanduser()
        if not path.is_file():
            fail("ERR_FILE_NOT_FOUND", f"接口源文档不存在: {raw}")
        if path.suffix.lower() not in SUPPORTED:
            fail("ERR_FORMAT_UNSUPPORTED", f"不支持的接口文档格式: {path.suffix}")
        resolved.append(str(path.resolve()))

    reference = payload.get("reference_document")
    if reference is not None:
        if not isinstance(reference, str):
            fail("ERR_TYPE_INVALID", "reference_document 必须是字符串路径或 null")
        ref_path = Path(reference).expanduser()
        if not ref_path.is_file():
            fail("ERR_FILE_NOT_FOUND", f"参考文档不存在: {reference}")

    overwrite = payload.get("overwrite", False)
    if not isinstance(overwrite, bool):
        fail("ERR_TYPE_INVALID", "overwrite 必须是 boolean")

    output_path = payload.get("output_path")
    if output_path is not None and not isinstance(output_path, str):
        fail("ERR_TYPE_INVALID", "output_path 必须是字符串路径或 null")
    if output_path is None:
        first = Path(resolved[0])
        output = first.with_name(first.stem + "-分层架构总结.md")
    else:
        output = Path(output_path).expanduser()
    if output.exists() and not overwrite:
        fail("ERR_OUTPUT_EXISTS", f"输出已存在且 overwrite=false: {output}")

    return {
        "source_documents": resolved,
        "reference_document": str(Path(reference).resolve()) if reference else None,
        "output_path": str(output.resolve()),
        "overwrite": overwrite,
        "audience": payload.get("audience", "应用开发者和初学者"),
        "architecture_model": payload.get("architecture_model", "auto"),
    }


def main() -> None:
    if len(sys.argv) != 2:
        fail("ERR_MISSING_INPUT", "用法: validate_inputs.py <input.json>")
    input_path = Path(sys.argv[1])
    if not input_path.is_file():
        fail("ERR_FILE_NOT_FOUND", f"输入 JSON 不存在: {input_path}")
    try:
        payload = json.loads(input_path.read_text(encoding="utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail("ERR_TYPE_INVALID", f"输入 JSON 无效: {exc}")
    print(json.dumps(response("success", data=validate(payload)), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

