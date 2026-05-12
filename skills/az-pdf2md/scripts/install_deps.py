"""依赖检测与安装脚本。检查并安装 pdfplumber、Pillow、pytesseract 等核心依赖。"""
import subprocess
import sys
import importlib

# (pip包名, 导入名, 是否必需)
DEPENDENCIES = [
    ("pdfplumber", "pdfplumber", True),
    ("Pillow", "PIL", True),
    ("pytesseract", "pytesseract", False),  # OCR，可选
]

# 可选重型依赖
OPTIONAL_HEAVY = [
    ("magic-pdf[full]", "magic_pdf", False),  # MinerU，~2GB
]


def check_package(import_name: str) -> bool:
    """检查包是否可导入。"""
    try:
        importlib.import_module(import_name)
        return True
    except ImportError:
        return False


def install_package(pip_spec: str) -> bool:
    """使用 pip 安装包，返回是否成功。"""
    try:
        result = subprocess.run(
            [sys.executable, "-m", "pip", "install", pip_spec, "-q"],
            capture_output=True,
            text=True,
            timeout=300,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, Exception):
        return False


def verify_magic_pdf_cli() -> bool:
    """验证 magic-pdf CLI 是否可用。"""
    try:
        result = subprocess.run(
            ["magic-pdf", "--help"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        return result.returncode == 0
    except (FileNotFoundError, subprocess.TimeoutExpired, Exception):
        return False


def main(install_heavy: bool = False) -> dict:
    """
    检查并安装依赖。

    Args:
        install_heavy: 是否尝试安装 MinerU 等重型依赖

    Returns:
        {"installed": [...], "failed": [...], "skipped": [...]}
    """
    results = {"installed": [], "failed": [], "skipped": []}

    all_deps = DEPENDENCIES[:]
    if install_heavy:
        all_deps.extend(OPTIONAL_HEAVY)

    for pip_spec, import_name, required in all_deps:
        if check_package(import_name):
            results["installed"].append(pip_spec)
            print(f"  [OK] {pip_spec}")
            continue

        if not required:
            results["skipped"].append(pip_spec)
            print(f"  [SKIP] {pip_spec} (可选，未安装)")
            continue

        print(f"  [INSTALL] {pip_spec} ...")
        if install_package(pip_spec):
            results["installed"].append(pip_spec)
            print(f"  [OK] {pip_spec}")
        else:
            results["failed"].append(pip_spec)
            print(f"  [FAIL] {pip_spec} - 安装失败，请手动执行: pip install {pip_spec}")

    # 验证 MinerU CLI（如果安装了的话）
    if install_heavy and check_package("magic_pdf"):
        if verify_magic_pdf_cli():
            print("  [OK] magic-pdf CLI 可用")
        else:
            print("  [WARN] magic-pdf CLI 不可用，MinerU 转换模式将无法使用")

    return results


if __name__ == "__main__":
    heavy = "--heavy" in sys.argv
    print("检查依赖...")
    result = main(install_heavy=heavy)
    print(f"\n已安装: {len(result['installed'])}, 失败: {len(result['failed'])}, 跳过: {len(result['skipped'])}")
    if result["failed"]:
        sys.exit(1)
