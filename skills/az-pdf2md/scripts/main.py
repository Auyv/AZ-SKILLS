"""主编排脚本。串联依赖检查、路径验证、PDF转换、MD优化的完整流水线。"""
import sys
import time
from pathlib import Path
from typing import Dict, List

# 确保 scripts 目录在 path 中
_SCRIPT_DIR = Path(__file__).parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from install_deps import main as check_deps
from validate_paths import parse_paths, validate_all
from convert_pdf import convert_pdf


def run_pipeline(pdf_paths_str: str, mode: str = "auto", install_heavy: bool = False) -> List[Dict]:
    """
    执行完整的 PDF 转 MD 流水线。

    Args:
        pdf_paths_str: 逗号分隔的 PDF 路径字符串
        mode: 转换模式 "auto" | "pdfplumber" | "mineru"
        install_heavy: 是否安装 MinerU 等重型依赖

    Returns:
        每个文件的结果列表: [{"pdf": str, "md": str, "success": bool, "method": str, "error": str, "pages": int}]
    """
    results = []

    # Step 1: 依赖检查
    print("=" * 50)
    print("Step 1/4: 检查依赖")
    print("=" * 50)
    deps = check_deps(install_heavy=install_heavy)
    if deps["failed"]:
        print(f"\n警告: 以下依赖安装失败: {', '.join(deps['failed'])}")
        print("部分功能可能不可用，继续执行...")

    # Step 2: 路径验证
    print("\n" + "=" * 50)
    print("Step 2/4: 验证路径")
    print("=" * 50)
    paths = parse_paths(pdf_paths_str)
    if not paths:
        print("错误: 未提供有效的 PDF 路径")
        return [{"pdf": "", "md": "", "success": False, "method": "", "error": "未提供PDF路径", "pages": 0}]

    validation = validate_all(paths)
    for p in validation["valid"]:
        print(f"  [OK] {p}")
    for p, err in validation["errors"]:
        print(f"  [ERR] {p}: {err}")

    if not validation["valid"]:
        print("\n错误: 没有有效的 PDF 文件可处理")
        return [{"pdf": p, "md": "", "success": False, "method": "", "error": err, "pages": 0}
                for p, err in validation["errors"]]

    # Step 3 & 4: 逐个转换和优化
    print("\n" + "=" * 50)
    print(f"Step 3/4: 转换 PDF ({len(validation['valid'])} 个文件)")
    print("=" * 50)

    for pdf_path in validation["valid"]:
        pdf_p = Path(pdf_path)
        md_path = str(pdf_p.with_suffix(".md"))

        print(f"\n  转换: {pdf_p.name}")
        start_time = time.time()

        # 转换
        result = convert_pdf(pdf_path, md_path, mode)
        elapsed = time.time() - start_time

        if result["success"]:
            # 优化
            print(f"  优化: {pdf_p.stem}.md")
            try:
                from optimize_md import optimize_md_file
                optimize_md_file(result["output"])
            except Exception as e:
                print(f"  [WARN] MD优化失败(已保留原始转换): {e}")

            print(f"  [OK] {pdf_p.stem}.md ({result['method']}, {result['pages']}页, {elapsed:.1f}秒)")
        else:
            print(f"  [FAIL] {result['error']}")

        results.append({
            "pdf": pdf_path,
            "md": md_path if result["success"] else "",
            "success": result["success"],
            "method": result["method"],
            "error": result["error"],
            "pages": result["pages"],
        })

    # 汇总错误的文件
    for p, err in validation["errors"]:
        results.append({"pdf": p, "md": "", "success": False, "method": "", "error": err, "pages": 0})

    return results


def print_summary(results: List[Dict]):
    """输出转换结果摘要。"""
    print("\n" + "=" * 50)
    print("Step 4/4: 转换结果摘要")
    print("=" * 50)

    total = len(results)
    success = sum(1 for r in results if r["success"])
    failed = total - success

    print(f"\n  总计: {total} 个文件")
    print(f"  成功: {success} 个")
    print(f"  失败: {failed} 个")

    if success > 0:
        print(f"\n  成功文件:")
        for r in results:
            if r["success"]:
                print(f"    {r['pdf']}")
                print(f"      → {r['md']}")

    if failed > 0:
        print(f"\n  失败文件:")
        for r in results:
            if not r["success"]:
                print(f"    {r['pdf']}")
                print(f"      原因: {r['error']}")

    print()
    return {"total": total, "success": success, "failed": failed}


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("PDF转MD工具 - 将PDF文件转换为大模型友好的Markdown文档")
        print()
        print("用法:")
        print('  python main.py "file.pdf"')
        print('  python main.py "file1.pdf, file2.pdf, file3.pdf"')
        print('  python main.py "file.pdf" --mode mineru')
        print('  python main.py "file.pdf" --heavy')
        print()
        print("参数:")
        print("  --mode <auto|pdfplumber|ocr|mineru>  转换模式 (默认: auto)")
        print("  --heavy                          安装MinerU等重型依赖")
        sys.exit(0)

    # 解析参数
    pdf_input = sys.argv[1]
    mode = "auto"
    heavy = False

    for i, arg in enumerate(sys.argv[2:], 2):
        if arg == "--mode" and i + 1 < len(sys.argv):
            mode = sys.argv[i + 1]
        elif arg == "--heavy":
            heavy = True

    results = run_pipeline(pdf_input, mode=mode, install_heavy=heavy)
    summary = print_summary(results)

    sys.exit(0 if summary["failed"] == 0 else 1)
