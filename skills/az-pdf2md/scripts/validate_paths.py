"""路径解析与验证脚本。解析逗号分隔的 PDF 路径，验证文件存在性和格式。"""
from pathlib import Path
from typing import List, Dict, Tuple


def parse_paths(input_str: str) -> List[str]:
    """
    解析逗号分隔的路径字符串。

    Args:
        input_str: 逗号分隔的文件路径，如 "a.pdf, b.pdf, c.pdf"

    Returns:
        去除空白后的路径列表
    """
    if not input_str or not input_str.strip():
        return []
    paths = [p.strip().strip('"').strip("'") for p in input_str.split(",")]
    return [p for p in paths if p]


def validate_pdf(path_str: str) -> Tuple[bool, str]:
    """
    验证单个 PDF 路径。

    Args:
        path_str: 文件路径字符串

    Returns:
        (是否有效, 错误信息或空字符串)
    """
    p = Path(path_str)

    if not p.exists():
        return False, f"文件不存在: {path_str}"

    if not p.is_file():
        return False, f"不是文件: {path_str}"

    if p.suffix.lower() != ".pdf":
        return False, f"不是PDF文件 (后缀为 {p.suffix}): {path_str}"

    if p.stat().st_size == 0:
        return False, f"文件为空: {path_str}"

    # 尝试读取文件头，确认是 PDF
    try:
        with open(p, "rb") as f:
            header = f.read(5)
            if header != b"%PDF-":
                return False, f"文件内容不是有效PDF格式: {path_str}"
    except (PermissionError, OSError) as e:
        return False, f"无法读取文件: {path_str} ({e})"

    return True, ""


def validate_all(paths: List[str]) -> Dict[str, list]:
    """
    批量验证路径。

    Args:
        paths: 路径列表

    Returns:
        {"valid": [有效路径列表], "errors": [(路径, 错误信息)列表]}
    """
    result = {"valid": [], "errors": []}
    for path_str in paths:
        ok, err = validate_pdf(path_str)
        if ok:
            result["valid"].append(str(Path(path_str).resolve()))
        else:
            result["errors"].append((path_str, err))
    return result


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("用法: python validate_paths.py <pdf路径1, pdf路径2, ...>")
        sys.exit(1)
    input_str = sys.argv[1]
    paths = parse_paths(input_str)
    result = validate_all(paths)
    print(f"有效: {len(result['valid'])}, 无效: {len(result['errors'])}")
    for p in result["valid"]:
        print(f"  [OK] {p}")
    for p, err in result["errors"]:
        print(f"  [ERR] {p}: {err}")
