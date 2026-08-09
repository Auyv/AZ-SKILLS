"""
generate_api_list.py - 生成接口清单 .md 文档
"""
from pathlib import Path
from datetime import datetime
from parse_md_api import parse_api_doc, format_module_breakdown


def load_template(template_path):
    """加载模板"""
    return Path(template_path).read_text(encoding='utf-8')


def render_function_table(functions):
    """渲染函数表"""
    if not functions:
        return "（无）\n"
    lines = ["| 函数 | 功能 | 返回值 |", "|------|------|--------|"]
    for func in functions:
        proto = func.get("proto", func.get("name", ""))
        desc = func.get("desc", "")
        ret = func.get("returns", "")
        lines.append(f"| `{proto}` | {desc} | {ret} |")
    return "\n".join(lines) + "\n"


def render_module_breakdown_table(breakdown):
    """渲染模块分布表"""
    lines = ["| 模块 | 接口数 |", "|------|--------|"]
    for mod, count in breakdown.items():
        lines.append(f"| {mod} | {count} |")
    lines.append(f"| **合计** | **{sum(breakdown.values())}** |")
    return "\n".join(lines) + "\n"


def render_error_code_table(error_codes):
    """渲染错误码表"""
    if not error_codes:
        return "（无）\n"
    lines = ["| 错误码 | 宏名 | 描述 |", "|--------|------|------|"]
    for ec in error_codes:
        name = ec.get("name", "")
        value = ec.get("value", "")
        lines.append(f"| {value} | `{name}` | |")
    return "\n".join(lines) + "\n"


def render_macro_table(macros):
    """渲染宏定义表"""
    if not macros:
        return "（无）\n"
    lines = ["| 宏 | 值 | 描述 |", "|----|----|------|"]
    for mc in macros:
        name = mc.get("name", "")
        value = mc.get("value", "")
        lines.append(f"| `{name}` | `{value}` | |")
    return "\n".join(lines) + "\n"


def generate(md_path, output_path, title=None):
    """生成接口清单 .md"""
    md_path = Path(md_path)
    if not md_path.exists():
        raise FileNotFoundError(f"输入文件不存在: {md_path}")

    result = parse_api_doc(md_path)
    if "error" in result:
        raise RuntimeError(result["error"])

    modules = result["modules"]
    breakdown = format_module_breakdown(modules)
    api_count = sum(breakdown.values())

    error_codes = modules.get("__ERROR_CODES__", {}).get("values", [])
    macros = modules.get("__MACROS__", {}).get("values", [])

    title = title or f"{md_path.stem} 接口清单"

    content = f"# {title}\n\n"
    content += f"**源文档**：`{md_path.name}`\n"
    content += f"**接口总数**：{api_count} 个\n"
    content += f"**生成时间**：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n"
    content += "---\n\n"

    # 1. 接口总览
    content += "## 1. 接口总览\n\n"
    content += render_module_breakdown_table(breakdown)
    content += "\n---\n\n"

    # 2-N. 各模块
    idx = 2
    for mod_name, mod_data in modules.items():
        if mod_name.startswith("__"):
            continue
        content += f"## {idx}. {mod_name}（{breakdown.get(mod_name, 0)} 个）\n\n"
        content += render_function_table(mod_data.get("functions", []))
        content += "\n---\n\n"
        idx += 1

    # 错误码
    if error_codes:
        content += f"## {idx}. 错误码\n\n"
        content += render_error_code_table(error_codes)
        content += "\n---\n\n"
        idx += 1

    # 宏定义
    if macros:
        content += f"## {idx}. 关键宏定义\n\n"
        content += render_macro_table(macros)
        content += "\n---\n\n"
        idx += 1

    # 备注
    content += f"## {idx}. 备注\n\n"
    content += "1. 本清单由 `az-api-list-from-pdf` 自动生成\n"
    content += "2. 接口数统计基于函数原型/参数/返回值完整识别\n"
    content += "3. 详细使用流程请参考原文档\n"

    output_path = Path(output_path)
    output_path.write_text(content, encoding='utf-8')

    return {
        "output_path": str(output_path),
        "api_count": api_count,
        "module_breakdown": breakdown,
        "summary": f"提取完成：共 {api_count} 个 API，输出至 {output_path}"
    }


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 3:
        print("Usage: python generate_api_list.py <input.md> <output.md> [title]")
        sys.exit(1)

    title = sys.argv[3] if len(sys.argv) > 3 else None
    result = generate(sys.argv[1], sys.argv[2], title)
    print(result["summary"])
