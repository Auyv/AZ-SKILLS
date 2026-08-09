"""
parse_md_api.py - 解析 Quectel API 参考手册 .md 文档
提取函数、结构体、枚举、错误码、宏定义
"""
import re
from pathlib import Path


# 章节识别正则
RE_MODULE = re.compile(r'^#{2,4}\s*3\.\d+\.\s+(.+)$')
RE_FUNCTION = re.compile(r'^#{2,4}\s*3\.\d+\.\d+\.\s+(\w+)\s*$')
RE_PROTOTYPE = re.compile(r'^#{2,4}\s*((?:int|void|char\s*\*|uint\d+_t\s*\*?|QTEE_\w+\s*\*)\s+\w+\([^)]*\))$')
RE_PARAM = re.compile(r'^([a-zA-Z_][a-zA-Z0-9_]*)\s*[:：]\s*$')
RE_PARAM_DIR = re.compile(r'\[(In|Out|In/Out)\]')
RE_RETURN_OK = re.compile(r'^0\s+函数执行成功')
RE_RETURN_FAIL = re.compile(r'^负值\s+函数执行失败')
RE_STRUCT = re.compile(r'^typedef\s+struct\s*(\{)?')
RE_ENUM = re.compile(r'^typedef\s+enum\s*(\{)?')
RE_MACRO = re.compile(r'^#define\s+(\w+)\s+(.+)$')
RE_ENUM_VAL = re.compile(r'^(\w+)\s*=\s*(\d+),?\s*$')
RE_FUNC_INNER = re.compile(r'^###\s+((?:int|void|char\s*\*|uint\d+_t\s*\*?|QTEE_\w+\s*\*)\s+\w+\([^)]*\))$')


def parse_api_doc(md_path):
    """解析 .md 提取所有 API 信息"""
    md_path = Path(md_path)
    if not md_path.exists():
        return {"error": "ERR_FILE_NOT_FOUND", "modules": {}}

    content = md_path.read_text(encoding='utf-8')
    lines = content.split('\n')

    modules = {}
    current_module = None
    current_function = None
    current_struct = None
    current_enum = None

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # 模块识别
        m = RE_MODULE.match(line)
        if m:
            current_module = m.group(1)
            modules.setdefault(current_module, {
                "functions": [],
                "structs": [],
                "enums": []
            })
            i += 1
            continue

        # 函数标题
        m = RE_FUNCTION.match(line)
        if m and current_module:
            current_function = {
                "name": m.group(1),
                "proto": "",
                "desc": "",
                "params": [],
                "returns": ""
            }
            i += 1
            continue

        # 函数原型（标题下第一行）
        m = RE_FUNC_INNER.match(line) or RE_PROTOTYPE.match(line)
        if m and current_function and not current_function["proto"]:
            current_function["proto"] = m.group(1).strip()
            i += 1
            continue

        # 错误码枚举值
        m = RE_ENUM_VAL.match(line)
        if m:
            modules.setdefault("__ERROR_CODES__", {"values": []})
            modules["__ERROR_CODES__"]["values"].append({
                "name": m.group(1),
                "value": int(m.group(2))
            })

        # 宏定义
        m = RE_MACRO.match(line)
        if m:
            modules.setdefault("__MACROS__", {"values": []})
            modules["__MACROS__"]["values"].append({
                "name": m.group(1),
                "value": m.group(2)
            })

        i += 1

    return {"modules": modules}


def format_module_breakdown(modules):
    """格式化模块分布统计"""
    breakdown = {}
    for name, data in modules.items():
        if name.startswith("__"):
            continue
        breakdown[name] = len(data.get("functions", []))
    return breakdown


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python parse_md_api.py <md_path>")
        sys.exit(1)

    result = parse_api_doc(sys.argv[1])
    if "error" in result:
        print(f"错误: {result['error']}")
        sys.exit(1)

    breakdown = format_module_breakdown(result["modules"])
    total = sum(breakdown.values())
    print(f"模块数: {len(breakdown)}, 函数总数: {total}")
    for mod, count in breakdown.items():
        print(f"  {mod}: {count}")
