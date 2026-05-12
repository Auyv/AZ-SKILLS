"""核心 PDF 转换脚本。支持 pdfplumber（主）、OCR、MinerU（可选）三种转换模式。"""
import io
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, List, Optional

try:
    import pdfplumber
except ImportError:
    pdfplumber = None

try:
    from PIL import Image
except ImportError:
    Image = None

try:
    import pytesseract
except ImportError:
    pytesseract = None

# --- 内部工具函数 ---

def _is_scanned_pdf(pdf_path: str, sample_pages: int = 3) -> bool:
    """
    检测整个 PDF 是否为扫描件。
    采样前几页，如果都没有文字字符但有图片，则判定为扫描件。
    """
    if pdfplumber is None:
        return False
    try:
        with pdfplumber.open(pdf_path) as pdf:
            pages_to_check = min(sample_pages, len(pdf.pages))
            scanned_count = 0
            for i in range(pages_to_check):
                page = pdf.pages[i]
                chars = page.chars or []
                images = page.images or []
                if len(chars) == 0 and len(images) > 0:
                    scanned_count += 1
            # 超过一半的采样页为扫描件，则认为整个PDF是扫描件
            return scanned_count > pages_to_check / 2
    except Exception:
        return False


def _is_scanned_page(page) -> bool:
    """判断单个页面是否为扫描件（图片为主，文字极少）。"""
    text = page.extract_text() or ""
    if len(text.strip()) < 50:
        if page.images:
            return True
    return False


def _get_font_sizes(page) -> list:
    """提取页面中所有字符的字体大小。"""
    sizes = []
    for char in (page.chars or []):
        size = char.get("size")
        if size:
            sizes.append(round(size, 1))
    return sizes


def _detect_heading_level(font_size: float, median_size: float) -> Optional[int]:
    """根据字体大小相对于中位数的比例推断标题层级。"""
    if font_size <= median_size:
        return None
    ratio = font_size / median_size
    if ratio >= 1.8:
        return 1
    elif ratio >= 1.4:
        return 2
    elif ratio >= 1.15:
        return 3
    return None


def _is_monospace(char: dict) -> bool:
    """判断字符是否为等宽字体。"""
    fontname = char.get("fontname", "").lower()
    mono_keywords = ["mono", "courier", "consol", "code", "cour"]
    return any(kw in fontname for kw in mono_keywords)


def _looks_like_code(text: str) -> bool:
    """判断文本行是否看起来像代码。"""
    code_patterns = [
        r"^\s*(def|class|import|from|if|for|while|return|try|except|with|async|await)\b",
        r"^\s*(function|var|let|const|export|return|if|for|while|try|catch)\b",
        r"^\s*(public|private|protected|static|void|int|String|bool)\b",
        r"[{}\[\]();]$",
        r"^\s*//",
        r"^\s*/\*",
        r"^\s*\*",
        r"=>\s*\{",
        r"^\s*@\w+",
        r"import\s+[\w.]+",
        r"from\s+[\w.]+\s+import",
    ]
    for pat in code_patterns:
        if re.search(pat, text):
            return True
    return False


def _looks_like_json(text: str) -> bool:
    """判断文本是否像 JSON 片段。"""
    stripped = text.strip()
    return (stripped.startswith("{") or stripped.startswith("[") or
            stripped.startswith('"') or '":' in stripped or '":"' in stripped)


# --- PDF 加密检测 ---

def detect_encrypted(pdf_path: str) -> bool:
    """检测 PDF 是否加密。"""
    try:
        with open(pdf_path, "rb") as f:
            header = f.read(1024)
            if b"/Encrypt" in header:
                return True
        # 进一步尝试 pdfplumber 打开
        with pdfplumber.open(pdf_path) as pdf:
            if pdf.is_encrypted:
                return True
    except Exception:
        pass
    return False


# --- pdfplumber 转换（主模式） ---

def convert_with_pdfplumber(pdf_path: str, output_path: str) -> Dict:
    """
    使用 pdfplumber 将 PDF 转换为 Markdown。

    Args:
        pdf_path: PDF 文件路径
        output_path: 输出 MD 文件路径

    Returns:
        {"success": bool, "method": "pdfplumber", "output": str, "error": str}
    """
    if pdfplumber is None:
        return {"success": False, "method": "pdfplumber", "output": "", "error": "pdfplumber 未安装"}

    try:
        pages_md = []

        with pdfplumber.open(pdf_path) as pdf:
            total_pages = len(pdf.pages)
            all_font_sizes = []

            # 第一遍：收集字体大小统计
            for page in pdf.pages:
                all_font_sizes.extend(_get_font_sizes(page))

            median_size = sorted(all_font_sizes)[len(all_font_sizes) // 2] if all_font_sizes else 12.0

            # 第二遍：逐页提取内容
            for i, page in enumerate(pdf.pages):
                page_content = _extract_page_content(page, i + 1, total_pages, median_size)
                if page_content.strip():
                    pages_md.append(page_content)

        md_content = "\n\n".join(pages_md)

        # 写入文件
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(md_content)

        return {"success": True, "method": "pdfplumber", "output": output_path, "error": ""}

    except Exception as e:
        return {"success": False, "method": "pdfplumber", "output": "", "error": str(e)}


def _extract_page_content(page, page_num: int, total_pages: int, median_size: float) -> str:
    """提取单页内容并转换为 Markdown。"""
    lines = []
    current_block = []  # 用于收集连续的代码行
    in_code_block = False
    in_json_block = False

    # 提取文本
    text = page.extract_text() or ""
    if not text.strip():
        return ""

    # 提取表格
    tables = page.extract_tables() or []

    # 逐行处理文本
    for line in text.split("\n"):
        stripped = line.strip()
        if not stripped:
            # 空行：结束当前代码块
            if current_block:
                if in_json_block:
                    lines.append("```json")
                else:
                    lines.append("```")
                lines.extend(current_block)
                lines.append("```")
                lines.append("")
                current_block = []
                in_code_block = False
                in_json_block = False
            lines.append("")
            continue

        # 检测标题（通过字体大小 —— 这里用启发式：全大写或特定前缀）
        heading = _detect_heading_from_text(stripped, median_size, page)
        if heading:
            if current_block:
                lines.append("```")
                lines.extend(current_block)
                lines.append("```")
                lines.append("")
                current_block = []
                in_code_block = False
            lines.append(heading)
            continue

        # 检测代码行
        if _looks_like_code(stripped) or _looks_like_json(stripped):
            if not in_code_block:
                in_code_block = True
                in_json_block = _looks_like_json(stripped)
            current_block.append(stripped)
            continue

        # 普通文本
        if current_block:
            if in_json_block:
                lines.append("```json")
            else:
                lines.append("```")
            lines.extend(current_block)
            lines.append("```")
            lines.append("")
            current_block = []
            in_code_block = False
            in_json_block = False

        lines.append(stripped)

    # 处理末尾的代码块
    if current_block:
        if in_json_block:
            lines.append("```json")
        else:
            lines.append("```")
        lines.extend(current_block)
        lines.append("```")

    # 添加表格
    for table in tables:
        if table and len(table) > 0:
            table_md = _format_table(table)
            if table_md:
                lines.append("")
                lines.append(table_md)
                lines.append("")

    return "\n".join(lines)


def _detect_heading_from_text(text: str, median_size: float, page) -> Optional[str]:
    """从文本内容推断标题。"""
    # 检查页面中的字符字体大小
    heading_chars = []
    for char in (page.chars or []):
        char_text = char.get("text", "")
        if char_text and char_text in text:
            size = char.get("size", 0)
            heading_chars.append(size)

    if heading_chars:
        avg_size = sum(heading_chars) / len(heading_chars)
        level = _detect_heading_level(avg_size, median_size)
        if level:
            return f"{'#' * level} {text}"

    # 启发式：全大写短行可能是标题
    if text.isupper() and len(text) < 80 and len(text) > 3:
        return f"## {text.title()}"

    # 常见标题模式
    heading_patterns = [
        (r"^(\d+\.?\d*\.?\d*)\s+(.+)$", None),  # "1. Title" 或 "1.1 Title"
        (r"^(Chapter|CHAPTER)\s+(\d+)", None),
        (r"^(Section|SECTION)\s+(\d+)", None),
        (r"^第[一二三四五六七八九十\\d]+[章节部分]", None),
    ]
    for pat, _ in heading_patterns:
        m = re.match(pat, text)
        if m:
            # 根据数字层级确定标题级别
            prefix = m.group(1) if m.lastindex >= 1 else ""
            dots = prefix.count(".")
            level = min(dots + 1, 4) if dots > 0 else (2 if prefix.startswith(("C", "S", "第")) else 2)
            return f"{'#' * level} {text}"

    return None


def _format_table(table: list) -> str:
    """将 pdfplumber 表格数据格式化为 Markdown 表格。"""
    if not table or len(table) < 2:
        return ""

    # 清理单元格
    cleaned = []
    for row in table:
        cleaned_row = []
        for cell in row:
            cell_text = str(cell).replace("\n", " ").strip() if cell else ""
            # 转义管道符
            cell_text = cell_text.replace("|", "\\|")
            cleaned_row.append(cell_text)
        cleaned.append(cleaned_row)

    # 确保所有行长度一致
    max_cols = max(len(r) for r in cleaned)
    for row in cleaned:
        while len(row) < max_cols:
            row.append("")

    # 生成 Markdown 表格
    header = cleaned[0]
    separator = ["---"] * max_cols
    lines = [
        "| " + " | ".join(header) + " |",
        "| " + " | ".join(separator) + " |",
    ]
    for row in cleaned[1:]:
        lines.append("| " + " | ".join(row) + " |")

    return "\n".join(lines)


# --- OCR 转换（扫描件处理） ---

def _page_to_image(pdf_path: str, page_index: int, dpi: int = 300):
    """将 PDF 页面渲染为 PIL Image。使用 pdfplumber 的 to_image。"""
    if pdfplumber is None or Image is None:
        return None
    try:
        with pdfplumber.open(pdf_path) as pdf:
            page = pdf.pages[page_index]
            # pdfplumber 的 to_image 返回 PageImage 对象
            img = page.to_image(resolution=dpi)
            # 转为 PIL Image
            return img.original
    except Exception:
        return None


def convert_with_ocr(pdf_path: str, output_path: str, lang: str = "chi_sim+eng") -> Dict:
    """
    使用 OCR（pytesseract）将扫描件 PDF 转换为 Markdown。

    Args:
        pdf_path: PDF 文件路径
        output_path: 输出 MD 文件路径
        lang: Tesseract 语言包，默认中文简体+英文

    Returns:
        {"success": bool, "method": "ocr", "output": str, "error": str}
    """
    if pytesseract is None:
        return {"success": False, "method": "ocr", "output": "", "error": "pytesseract 未安装。请执行: pip install pytesseract"}
    if Image is None:
        return {"success": False, "method": "ocr", "output": "", "error": "Pillow 未安装。请执行: pip install Pillow"}
    if pdfplumber is None:
        return {"success": False, "method": "ocr", "output": "", "error": "pdfplumber 未安装"}

    try:
        pages_md = []

        with pdfplumber.open(pdf_path) as pdf:
            total_pages = len(pdf.pages)

            for i, page in enumerate(pdf.pages):
                print(f"    OCR 处理第 {i+1}/{total_pages} 页...", end="\r")

                # 方法1：直接用 pdfplumber 的 to_image
                try:
                    img_obj = page.to_image(resolution=300)
                    pil_img = img_obj.original
                except Exception:
                    # 方法2：如果 to_image 失败，尝试直接从 PDF 渲染
                    pil_img = _page_to_image(pdf_path, i)

                if pil_img is None:
                    continue

                # OCR 识别
                try:
                    text = pytesseract.image_to_string(pil_img, lang=lang)
                except pytesseract.TesseractNotFoundError:
                    return {
                        "success": False, "method": "ocr", "output": "",
                        "error": "Tesseract OCR 引擎未安装。请安装: https://github.com/UB-Mannheim/tesseract/wiki"
                    }
                except Exception as e:
                    # 语言包不存在时尝试仅用英文
                    try:
                        text = pytesseract.image_to_string(pil_img, lang="eng")
                    except Exception:
                        text = ""

                if text.strip():
                    pages_md.append(text.strip())

        if not pages_md:
            return {"success": False, "method": "ocr", "output": "", "error": "OCR 未能识别出任何文字"}

        md_content = "\n\n---\n\n".join(pages_md)

        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(md_content)

        print()  # 清除 \r 行
        return {"success": True, "method": "ocr", "output": output_path, "error": ""}

    except Exception as e:
        return {"success": False, "method": "ocr", "output": "", "error": str(e)}


# --- MinerU 转换（可选高级模式） ---

def convert_with_mineru(pdf_path: str, output_path: str) -> Dict:
    """
    使用 MinerU (magic-pdf) 将 PDF 转换为 Markdown。

    Args:
        pdf_path: PDF 文件路径
        output_path: 输出 MD 文件路径

    Returns:
        {"success": bool, "method": "mineru", "output": str, "error": str}
    """
    pdf_stem = Path(pdf_path).stem

    # 检查 magic-pdf 是否可用
    try:
        subprocess.run(["magic-pdf", "--help"], capture_output=True, timeout=10)
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return {"success": False, "method": "mineru", "output": "", "error": "magic-pdf CLI 未安装。请执行: pip install magic-pdf[full]"}

    try:
        # 使用临时目录接收 MinerU 的嵌套输出
        with tempfile.TemporaryDirectory() as tmp_dir:
            result = subprocess.run(
                ["magic-pdf", "-p", pdf_path, "-o", tmp_dir, "-m", "auto"],
                capture_output=True,
                text=True,
                timeout=600,
            )

            if result.returncode != 0:
                return {"success": False, "method": "mineru", "output": "", "error": f"magic-pdf 执行失败: {result.stderr[:500]}"}

            # 查找输出的 MD 文件（嵌套结构: <tmp>/<stem>/auto/<stem>.md）
            md_candidates = list(Path(tmp_dir).rglob(f"{pdf_stem}.md"))
            if not md_candidates:
                md_candidates = list(Path(tmp_dir).rglob("*.md"))
            if not md_candidates:
                return {"success": False, "method": "mineru", "output": "", "error": "MinerU 未生成 MD 文件"}

            # 复制到目标位置
            src_md = md_candidates[0]
            Path(output_path).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(str(src_md), output_path)

            return {"success": True, "method": "mineru", "output": output_path, "error": ""}

    except subprocess.TimeoutExpired:
        return {"success": False, "method": "mineru", "output": "", "error": "MinerU 转换超时（>10分钟）"}
    except Exception as e:
        return {"success": False, "method": "mineru", "output": "", "error": str(e)}


# --- 主转换入口 ---

def convert_pdf(pdf_path: str, output_path: str, mode: str = "auto") -> Dict:
    """
    主转换入口。根据 mode 选择转换器。

    Args:
        pdf_path: PDF 文件路径
        output_path: 输出 MD 文件路径
        mode: "auto"（默认，pdfplumber优先）、"pdfplumber"、"mineru"

    Returns:
        {"success": bool, "method": str, "output": str, "error": str, "pages": int}
    """
    pdf_path = str(Path(pdf_path).resolve())
    output_path = str(Path(output_path).resolve())

    # 检测加密
    if detect_encrypted(pdf_path):
        return {
            "success": False, "method": "", "output": "", "error": "PDF已加密，请先解除加密后重试",
            "pages": 0
        }

    if mode == "mineru":
        result = convert_with_mineru(pdf_path, output_path)
    elif mode == "pdfplumber":
        result = convert_with_pdfplumber(pdf_path, output_path)
    elif mode == "ocr":
        result = convert_with_ocr(pdf_path, output_path)
    else:  # auto
        # 先检测是否为扫描件
        is_scanned = _is_scanned_pdf(pdf_path)
        if is_scanned:
            print("  [INFO] 检测到扫描件PDF，启用OCR模式")
            # 扫描件：OCR优先，MinerU备选
            result = convert_with_ocr(pdf_path, output_path)
            if not result["success"]:
                print("  [INFO] OCR失败，尝试MinerU...")
                result = convert_with_mineru(pdf_path, output_path)
        else:
            # 非扫描件：pdfplumber优先，MinerU备选
            result = convert_with_pdfplumber(pdf_path, output_path)
            if not result["success"]:
                result = convert_with_mineru(pdf_path, output_path)

    # 获取页数
    try:
        with pdfplumber.open(pdf_path) as pdf:
            result["pages"] = len(pdf.pages)
    except Exception:
        result["pages"] = 0

    return result


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python convert_pdf.py <pdf路径> [输出md路径] [模式:auto|pdfplumber|ocr|mineru]")
        sys.exit(1)

    pdf = sys.argv[1]
    pdf_path = Path(pdf)
    out = sys.argv[2] if len(sys.argv) > 2 else str(pdf_path.with_suffix(".md"))
    m = sys.argv[3] if len(sys.argv) > 3 else "auto"

    print(f"转换: {pdf}")
    print(f"模式: {m}")
    r = convert_pdf(pdf, out, m)
    if r["success"]:
        print(f"成功: {r['output']} (方法: {r['method']}, 页数: {r['pages']})")
    else:
        print(f"失败: {r['error']}")
        sys.exit(1)
