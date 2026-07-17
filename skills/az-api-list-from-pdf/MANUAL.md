# az-api-list-from-pdf 使用手册

> 给手动操作的用户参考。
> AI Agent 请阅读 [SKILL.md](SKILL.md)。

## 一、这是什么

本技能用于从 Quectel/移远模块的 **API 参考手册 PDF** 中，自动提取所有 API 函数、结构体、枚举、错误码、宏定义，整理为结构化 Markdown 接口清单。

**典型场景**：
- 拿到一份 `Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.pdf`
- 希望快速得到一份「接口清单 .md」，方便后续开发查阅

## 二、前置条件

1. 已安装 Python 3.8+
2. 已安装 `pdfplumber` 库（`pip install pdfplumber`）
3. 已加载 `az-pdf2md` 技能（用于 PDF→MD 转换）

## 三、5 分钟快速上手

### 方式 1：使用 AI Agent（推荐）

直接对 AI Agent 说：

```
帮我把 D:\模组资料\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.pdf 转成接口清单
```

Agent 会自动：
1. 调用 `az-pdf2md` 把 PDF 转成 .md
2. 调用 `az-api-list-from-pdf` 提取并整理接口清单
3. 在原 .md 末尾追加「接口清单索引」

### 方式 2：手动执行脚本

#### 步骤 1：PDF 转 MD（如已有 .md 跳过）

```bash
python "C:\Users\jpz\.agents\skills\az-skills\az-pdf2md\scripts\main.py" "<pdf_path>"
```

输出：`<pdf_path>.md`

#### 步骤 2：生成接口清单

```bash
python "C:\Users\jpz\.agents\skills\az-skills\az-api-list-from-pdf\scripts\generate_api_list.py" "<md_path>"
```

输出：`<md_path>_接口清单.md`

#### 步骤 3：追加索引到原 .md

```bash
python "C:\Users\jpz\.agents\skills\az-skills\az-api-list-from-pdf\scripts\append_index.py" "<md_path>" "<list_md_path>"
```

## 四、脚本详细说明

### 1. parse_md_api.py - 解析 .md 提取 API 信息

```python
from scripts.parse_md_api import parse_api_doc

result = parse_api_doc(md_path)
# 返回:
# {
#   "modules": {
#     "RSA": {
#       "functions": [...],
#       "structs": [...],
#       "enums": [...]
#     },
#     ...
#   },
#   "error_codes": [...],
#   "macros": [...]
# }
```

### 2. generate_api_list.py - 生成接口清单

```python
from scripts.generate_api_list import generate

generate(
    md_path="input.md",
    output_path="output_接口清单.md",
    title="QTEE API 接口清单"
)
```

### 3. append_index.py - 在原 .md 追加索引

```python
from scripts.append_index import append_index

append_index(
    md_path="input.md",
    list_md_path="input_接口清单.md",
    api_count=43
)
```

## 五、输出样例

输入：`Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.pdf`（62 页）

输出：
1. `Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.md`（103 KB）
2. `Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0_接口清单.md`（13 KB）

接口清单包含：
- 1 个总览表
- 4 个模块分类（RSA 15 / AES 12 / ECC 13 / 安全存储 3）
- 43 个函数表（签名+参数+返回值）
- 9 个错误码表
- 9 类关键枚举
- 12 个宏定义
- 4 类使用流程示例
- 5 条关键备注

## 六、常见问题

### Q1：识别不到 API 怎么办？

检查：
- PDF 是否为 API 参考手册类（包含"函数原型"、"参数"、"返回值"等关键词）
- PDF 是否已正确转成 .md（无乱码）
- 是否需要 OCR（扫描版 PDF）

### Q2：接口数量统计不准确？

部分接口可能在文档不同章节被重复提及。请以「函数概览」章节的统计为准。

### Q3：支持其他厂商吗？

支持所有结构相似的 API 文档。常见适配：
- Quectel（移远）API 参考手册 ✅
- Fibocom（广和通）AT 命令手册 ⚠️ 需调整解析规则
- SIMCom（芯讯通）AT 命令手册 ⚠️ 需调整解析规则

### Q4：处理一份手册要多久？

| 步骤 | 时长 |
|------|------|
| PDF → MD | 10-60 秒 |
| 解析 + 生成清单 | 5-15 秒 |
| 追加索引 | < 1 秒 |

## 七、扩展定制

如需自定义接口清单模板，编辑：

```
az-api-list-from-pdf/assets/api_list.template.md
```

支持的模板变量：
- `{title}` - 文档标题
- `{modules}` - 模块列表
- `{error_codes}` - 错误码列表
- `{macros}` - 宏定义列表
- `{api_count}` - 接口总数
- `{timestamp}` - 生成时间
