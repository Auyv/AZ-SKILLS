"""
append_index.py - 在原 .md 文档末尾追加「接口清单索引」链接
"""
from pathlib import Path
from datetime import datetime


INDEX_TEMPLATE = """

---

## {section_num}. 接口清单索引

本文档所有 API 接口已整理为独立清单文件，详见：
**`{list_md_name}`**

包含以下内容：
- 共 {api_count} 个 API 函数按模块分类
- 完整函数签名、参数、返回值
- 错误码、关键枚举、宏定义
- 关键使用流程示例

> 索引生成时间：{timestamp}
"""


def append_index(md_path, list_md_path, api_count, section_num=None):
    """在原 .md 末尾追加接口清单索引"""
    md_path = Path(md_path)
    list_md_path = Path(list_md_path)

    if not md_path.exists():
        raise FileNotFoundError(f"原 .md 不存在: {md_path}")
    if not list_md_path.exists():
        raise FileNotFoundError(f"清单 .md 不存在: {list_md_path}")

    content = md_path.read_text(encoding='utf-8')

    # 检测是否已存在索引
    if "## 接口清单索引" in content or "## . 接口清单索引" in content:
        return {"status": "skipped", "reason": "索引已存在"}

    # 智能选择章节号
    if section_num is None:
        import re
        existing = re.findall(r'^##\s+(\d+)\.', content, re.MULTILINE)
        max_num = max((int(n) for n in existing), default=0)
        section_num = max_num + 1

    index_content = INDEX_TEMPLATE.format(
        section_num=section_num,
        list_md_name=list_md_path.name,
        api_count=api_count,
        timestamp=datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    )

    # 追加到末尾
    with open(md_path, 'a', encoding='utf-8') as f:
        f.write(index_content)

    return {
        "status": "success",
        "md_path": str(md_path),
        "index_section": section_num
    }


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 3:
        print("Usage: python append_index.py <md_path> <list_md_path> [api_count]")
        sys.exit(1)

    api_count = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    result = append_index(sys.argv[1], sys.argv[2], api_count)
    print(result)
