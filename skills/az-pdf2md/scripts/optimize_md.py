"""Markdown 后处理优化脚本。清理水印、页码、页眉页脚，修复表格，包裹代码块。"""
import json
import re
from collections import Counter
from pathlib import Path
from typing import List


# 加载清理规则
_PATTERNS_FILE = Path(__file__).parent.parent / "assets" / "md_cleanup_patterns.json"
_PATTERNS = None


def _load_patterns() -> dict:
    """加载清理规则配置。"""
    global _PATTERNS
    if _PATTERNS is None:
        try:
            with open(_PATTERNS_FILE, "r", encoding="utf-8") as f:
                _PATTERNS = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            _PATTERNS = {"page_numbers": [], "watermarks": [], "extra_whitespace": {"max_consecutive_blank_lines": 2}}
    return _PATTERNS


def remove_page_numbers(content: str) -> str:
    """移除页码行和行尾嵌入的页码。"""
    patterns = _load_patterns().get("page_numbers", [])
    lines = content.split("\n")
    result = []
    for line in lines:
        stripped = line.strip()
        # 检查是否为独立页码行
        is_page_num = False
        for pat in patterns:
            if re.match(pat, stripped, re.IGNORECASE):
                is_page_num = True
                break
        if is_page_num:
            continue
        # 去除行尾嵌入的页码，如 "标题文字 23 / 46" 或 "标题文字 Page 23"
        cleaned = re.sub(r"\s+\d+\s*/\s*\d+\s*$", "", line)
        cleaned = re.sub(r"\s+Page\s+\d+\s*$", "", cleaned, flags=re.IGNORECASE)
        cleaned = re.sub(r"\s+第\s*\d+\s*页\s*$", "", cleaned)
        result.append(cleaned)
    return "\n".join(result)


def remove_watermarks(content: str) -> str:
    """移除水印文本。"""
    patterns = _load_patterns().get("watermarks", [])
    lines = content.split("\n")
    result = []
    for line in lines:
        stripped = line.strip()
        is_watermark = False
        for pat in patterns:
            if re.search(pat, stripped, re.IGNORECASE):
                is_watermark = True
                break
        if not is_watermark:
            result.append(line)
    return "\n".join(result)


def remove_headers_footers(content: str, pages: List[str] = None) -> str:
    """
    检测并移除跨页重复出现的页眉页脚。

    策略：如果某行在超过50%的"页面"中出现，且长度<120字符，则视为页眉/页脚。
    """
    if pages is None:
        # 按 form feed 或连续空行分页
        pages = re.split(r"\n{3,}", content)

    if len(pages) < 3:
        return content  # 页数太少，不做检测

    # 统计每行出现的页数
    line_page_count = Counter()
    for page_content in pages:
        seen_in_page = set()
        for line in page_content.split("\n"):
            stripped = line.strip()
            if stripped and len(stripped) < 120 and not stripped.startswith("#"):
                if stripped not in seen_in_page:
                    seen_in_page.add(stripped)
                    line_page_count[stripped] += 1

    # 找出重复率超过50%的行
    threshold = len(pages) * 0.5
    repeated_lines = {line for line, count in line_page_count.items() if count >= threshold}

    if not repeated_lines:
        return content

    # 移除这些重复行
    lines = content.split("\n")
    result = []
    for line in lines:
        if line.strip() not in repeated_lines:
            result.append(line)
    return "\n".join(result)


def fix_tables(content: str) -> str:
    """
    修复 Markdown 表格格式：
    - 确保表头分隔行存在
    - 修复分隔符错乱
    """
    lines = content.split("\n")
    result = []
    i = 0
    while i < len(lines):
        line = lines[i]
        # 检测表格行（以 | 开头和结尾）
        if line.strip().startswith("|") and line.strip().endswith("|"):
            table_lines = [line]
            i += 1
            while i < len(lines) and lines[i].strip().startswith("|") and lines[i].strip().endswith("|"):
                table_lines.append(lines[i])
                i += 1

            # 修复表格
            fixed = _fix_single_table(table_lines)
            result.extend(fixed)
        else:
            result.append(line)
            i += 1

    return "\n".join(result)


def _fix_single_table(table_lines: List[str]) -> List[str]:
    """修复单个表格。"""
    if len(table_lines) < 2:
        return table_lines

    # 检查第二行是否为分隔行
    second = table_lines[1].strip()
    if not re.match(r"^\|[\s\-:|]+\|$", second):
        # 缺少分隔行，插入一个
        # 计算列数
        cols = table_lines[0].count("|") - 1
        if cols > 0:
            separator = "| " + " | ".join(["---"] * cols) + " |"
            table_lines.insert(1, separator)
    else:
        # 修复现有分隔行
        cols = table_lines[0].count("|") - 1
        if cols > 0:
            separator = "| " + " | ".join(["---"] * cols) + " |"
            table_lines[1] = separator

    return table_lines


def wrap_code_blocks(content: str) -> str:
    """
    检测未包裹的代码内容并用 ``` 包裹。
    已有 ``` 包裹的代码块不会被重复处理。
    """
    lines = content.split("\n")
    result = []
    in_existing_block = False
    current_code = []
    code_lang = ""

    for line in lines:
        stripped = line.strip()

        # 跟踪已有的代码块
        if stripped.startswith("```"):
            if in_existing_block:
                # 结束已有代码块
                in_existing_block = False
                result.append(line)
                continue
            elif current_code:
                # 结束检测到的代码块
                result.append(f"```{code_lang}")
                result.extend(current_code)
                result.append("```")
                current_code = []
                code_lang = ""
                # 这行本身是新的代码块开始
                in_existing_block = True
                result.append(line)
                continue
            else:
                in_existing_block = True
                result.append(line)
                continue

        if in_existing_block:
            result.append(line)
            continue

        # 检测代码行
        if _is_code_line(stripped):
            if not current_code:
                code_lang = _detect_code_lang(stripped)
            current_code.append(line)
        else:
            if current_code:
                # 结束收集的代码块
                result.append(f"```{code_lang}")
                result.extend(current_code)
                result.append("```")
                result.append("")
                current_code = []
                code_lang = ""
            result.append(line)

    # 处理末尾的代码块
    if current_code:
        result.append(f"```{code_lang}")
        result.extend(current_code)
        result.append("```")

    return "\n".join(result)


def _is_code_line(text: str) -> bool:
    """判断文本行是否为代码。"""
    if not text:
        return False
    code_patterns = _load_patterns().get("code_indicators", [])
    for pat in code_patterns:
        if re.search(pat, text):
            return True
    return False


def _detect_code_lang(text: str) -> str:
    """检测代码语言。"""
    lang_patterns = {
        "python": [r"\bdef\s+\w+", r"\bimport\s+\w+", r"\bfrom\s+\w+\s+import", r"\bprint\s*\("],
        "java": [r"\bpublic\s+(static\s+)?void\s+", r"\bSystem\.out\.", r"\bclass\s+\w+\s*\{"],
        "javascript": [r"\bfunction\s+\w+", r"\bconst\s+\w+\s*=", r"\blet\s+\w+\s*=", r"=>\s*\{"],
        "json": [r"^\s*\{", r"\"\w+\"\s*:"],
        "bash": [r"^\$\s+", r"^#!/bin/bash", r"\becho\s+"],
        "xml": [r"<\w+[\s>]", r"</\w+>"],
    }
    for lang, patterns in lang_patterns.items():
        for pat in patterns:
            if re.search(pat, text):
                return lang
    return ""


def clean_extra_whitespace(content: str) -> str:
    """折叠多余的连续空行。"""
    max_blank = _load_patterns().get("extra_whitespace", {}).get("max_consecutive_blank_lines", 2)
    # 将超过 max_blank 个连续空行替换为 max_blank 个
    pattern = r"\n{" + str(max_blank + 1) + r",}"
    replacement = "\n" * (max_blank + 1)  # max_blank 个空行 = max_blank+1 个换行
    content = re.sub(pattern, replacement, content)
    # 去除首尾空白
    return content.strip() + "\n"


def optimize_md(content: str) -> str:
    """
    完整的 MD 优化流水线。按顺序执行所有清理步骤。

    Args:
        content: 原始 Markdown 内容

    Returns:
        优化后的 Markdown 内容
    """
    content = remove_page_numbers(content)
    content = remove_watermarks(content)
    content = remove_headers_footers(content)
    content = fix_tables(content)
    content = wrap_code_blocks(content)
    content = clean_extra_whitespace(content)
    return content


def optimize_md_file(input_path: str, output_path: str = None) -> str:
    """
    优化 MD 文件。

    Args:
        input_path: 输入 MD 文件路径
        output_path: 输出路径（默认覆盖原文件）

    Returns:
        输出文件路径
    """
    if output_path is None:
        output_path = input_path

    with open(input_path, "r", encoding="utf-8") as f:
        content = f.read()

    optimized = optimize_md(content)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(optimized)

    return output_path


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("用法: python optimize_md.py <md文件路径> [输出路径]")
        sys.exit(1)
    inp = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else None
    result = optimize_md_file(inp, out)
    print(f"优化完成: {result}")
