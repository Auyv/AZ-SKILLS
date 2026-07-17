---
name: az-api-list-from-pdf
description: 从 Quectel/移远模块技术 PDF（如 QuecOpen QTEE API 参考手册、SMS/SIM/UART 等 API 参考手册）中，提取所有 API 函数、结构体、枚举、错误码、宏定义，整理为结构清晰的大模型友好 Markdown 接口清单文档。
version: 1.0.0

inputs:
  required:
    - name: pdf_path
      type: string
      description: 已转好的 .md 文件路径（先经过 az-pdf2md 转换），或原始 PDF 路径
      validation: 必填；文件必须存在；扩展名为 .md 或 .pdf
  optional:
    - name: source_pdf_path
      type: string
      description: 原始 PDF 路径，若提供则先转换
      default: ""
    - name: output_dir
      type: string
      description: 接口清单输出目录，默认为 .md 文件同级目录
      default: ""
    - name: doc_title
      type: string
      description: 接口清单文档主标题，默认为 "<原文件名>_接口清单.md"
      default: ""
    - name: include_sections
      type: array
      description: 包含的章节过滤；空数组表示全部。常见值：["总览","函数清单","结构体","枚举","错误码","宏定义","使用流程","备注"]
      default: []

outputs:
  - name: output_md_path
    type: file
    description: 生成的接口清单 .md 文件绝对路径
    path: 同名 + "_接口清单.md"
  - name: api_count
    type: integer
    description: 提取到的 API 函数总数
  - name: module_breakdown
    type: object
    description: 按模块分组的接口数量统计，例如 {"RSA": 15, "AES": 12, "ECC": 13, "安全存储": 3}
  - name: summary
    type: string
    description: 转换摘要文本，列出输出文件路径、接口总数、模块分布

errors:
  - code: ERR_FILE_NOT_FOUND
    message: 输入的 .md 或 .pdf 文件不存在
    handling: 检查路径并重新提供；若为 PDF 请确认已通过 az-pdf2md 转换
  - code: ERR_NOT_API_DOC
    message: 文档中未识别到 API 接口特征（函数原型/结构体/枚举）
    handling: 提示用户提供 QuecOpen/移远 API 参考手册类 PDF
  - code: ERR_PARSE_FAILED
    message: Markdown 内容解析失败，无法识别章节结构
    handling: 检查文档是否被正确转换；可手动提供参考的章节结构
  - code: ERR_OUTPUT_WRITE
    message: 输出文件写入失败
    handling: 检查输出目录写权限
  - code: ERR_INVALID_INPUT
    message: 输入参数无效（空路径/非字符串）
    handling: 重新校验输入
---

# az-api-list-from-pdf

## 做什么

从 Quectel/移远模块的 API 参考手册类 PDF 中，**自动提取所有 API 接口**并整理为结构化、可直接被大模型或工程师消费的 Markdown 接口清单文档。

**核心能力**：
- 智能识别函数原型、参数、返回值、错误码
- 自动分类（按模块：RSA/AES/ECC/安全存储/UART/SMS/SIM/GPIO...）
- 提取结构体、枚举、宏定义、关键流程示例
- 输出带目录、模块索引、参数表的标准 Markdown
- 在原文档末尾追加「接口清单索引」反向链接

## 何时使用

1. 用户提供 Quectel AG35/EC25/EC200/EG915/BC95 等系列模块的 API 参考手册 PDF
2. 用户要求「整理接口」、「提取 API」、「生成接口清单」
3. 用户已完成 `az-pdf2md` 转换，希望对生成的 .md 做二次结构化整理
4. 关键词：「API 清单」「接口列表」「函数索引」「API 文档整理」

**典型应用对象**：
- Quectel_AG35系列_QuecOpen_QTEE_API_参考手册
- Quectel_AG35系列_QuecOpen_SMS_API_参考手册
- Quectel_AG35系列_QuecOpen_(U)SIM_API_参考手册
- Quectel_AG35系列_QuecOpen_GPIO_API_参考手册
- Quectel_AG35系列_QuecOpen_UART_应用指南
- 等所有 Quectel/移远 API 参考手册类文档

## 怎么做

### 流程概览

```
输入 PDF/.md
  ↓
[1] 检测文件类型（.pdf / .md）
  ↓
[2] 若是 PDF：调用 az-pdf2md 完成转换
  ↓
[3] 解析 .md 内容，识别章节结构（3.x.x 函数详解）
  ↓
[4] 按模块聚合：函数 / 结构体 / 枚举 / 错误码 / 宏
  ↓
[5] 生成结构化接口清单 .md
  ↓
[6] 在原 .md 末尾追加「接口清单索引」链接
  ↓
[7] 输出：路径 + 接口总数 + 模块分布 + 摘要
```

### 第一步：输入验证

```python
# 验证 PDF/.md 文件存在且可读
import os
def validate_input(pdf_path):
    if not pdf_path or not isinstance(pdf_path, str):
        return False, "ERR_INVALID_INPUT", "路径为空或非字符串"
    if not os.path.exists(pdf_path):
        return False, "ERR_FILE_NOT_FOUND", f"文件不存在: {pdf_path}"
    return True, None, None
```

### 第二步：PDF 转 MD（如需要）

若输入是 PDF，调用 `az-pdf2md` 技能完成 PDF→MD 转换，再进入下一步。

```bash
python "C:\Users\jpz\.agents\skills\az-skills\az-pdf2md\scripts\main.py" "<pdf_path>"
```

### 第三步：解析 .md 内容

读取 .md 内容，按以下模式识别：

| 模式 | 正则片段 | 含义 |
|------|----------|------|
| 模块标题 | `^### 3\.\d+\. (.+)$` | 如 `3.3.1. TrustZone RSA加密和解密` |
| 函数标题 | `^### 3\.\d+\.\d+\. (\w+)$` | 如 `3.3.1.1. QTEE_RSA_init` |
| 函数原型 | `^### \w+ \*?\w+\(.*\)$` 或 `^int \w+\(.*\)$` | 提取函数签名 |
| 参数块 | `^[a-z_]+：\s*\[(In\|Out)\]` | 提取方向、参数说明 |
| 返回值块 | `^0 函数执行成功` | 提取返回值含义 |
| 结构体 | `typedef struct` | 提取结构体定义 |
| 枚举 | `typedef enum` | 提取枚举定义 |
| 错误码 | `QTEE_ERR_\w+ = \d+` | 提取错误码 |
| 宏 | `#define \w+` | 提取宏定义 |

### 第四步：按模块聚合

将解析结果按文档章节结构组织：

```python
# 模块结构示例
modules = {
    "3.3.1 TrustZone RSA加密和解密": {
        "functions": [
            {"name": "QTEE_RSA_init", "proto": "QTEE_RSA *QTEE_RSA_init(void)",
             "desc": "分配QTEE_RSA结构体所需内存", "params": [], "returns": "..."},
            ...
        ],
        "structs": [...],
        "enums": [...],
    },
    ...
}
```

### 第五步：生成接口清单 .md

模板结构（参考 `assets/api_list.template.md`）：

```markdown
# <文档名> 接口清单

## 1. 接口总览
（按模块统计接口数）

## 2. <模块1>（N 个）
### 2.1 密钥管理
| 函数 | 功能 | 返回值 |
### 2.2 上下文
...
## 3. <模块2>（N 个）
...

## X. 错误码
| 错误码 | 宏名 | 描述 |

## X. 关键枚举
（RSA填充、AES操作、签名模式等）

## X. 关键宏定义
| 宏 | 值 | 描述 |

## X. 使用流程示例
### X.1 RSA 加密流程
### X.2 AES 加密流程
### X.3 ECC 签名流程
### X.4 安全存储流程

## X. 备注
```

### 第六步：在原 .md 追加索引

在原 .md 文档末尾追加：

```markdown
---

## X. 接口清单索引

本文档所有 API 接口已整理为独立清单文件，详见：
**`<原文件名>_接口清单.md`**

包含以下内容：
- N 个函数按模块分类（... ...）
- 完整函数签名、参数、返回值
- 错误码、关键枚举、宏定义
- N 类典型使用流程
```

### 第七步：输出摘要

```json
{
  "status": "success",
  "timestamp": "2026-07-09T...",
  "data": {
    "output_md_path": "D:\\...\\XXX_接口清单.md",
    "api_count": 43,
    "module_breakdown": {
      "RSA": 15,
      "AES": 12,
      "ECC": 13,
      "安全存储": 3
    },
    "summary": "提取完成：共 43 个 API（RSA 15 / AES 12 / ECC 13 / 安全存储 3），输出至 XXX_接口清单.md"
  }
}
```

## 异常场景

| 异常场景 | 输入 | 预期行为 |
|---------|------|---------|
| 空路径 | `pdf_path=""` | 返回 ERR_INVALID_INPUT |
| 文件不存在 | `pdf_path="X:\\nofile.pdf"` | 返回 ERR_FILE_NOT_FOUND |
| 非 API 文档 | 内容无函数原型/结构体 | 返回 ERR_NOT_API_DOC，提示用户提供 API 参考手册 |
| 加密 PDF | 加密的 PDF | 提示用户先解除加密 |
| 解析失败 | 章节结构混乱 | 返回 ERR_PARSE_FAILED 并标记已识别部分 |
| 输出目录无权限 | 写入只读目录 | 返回 ERR_OUTPUT_WRITE |
| 重复处理 | 文件已包含「接口清单索引」 | 跳过追加或询问用户确认 |

## 技能文件结构

```
az-api-list-from-pdf/
├── SKILL.md                    (本文件)
├── MANUAL.md                   (用户手动操作指南)
├── scripts/
│   ├── parse_md_api.py         # 解析 .md 提取 API 信息
│   ├── generate_api_list.py    # 生成接口清单 .md
│   ├── extract_modules.py      # 提取模块结构
│   ├── extract_structs.py      # 提取结构体/枚举
│   ├── extract_macros.py       # 提取宏定义
│   └── append_index.py         # 在原 .md 追加索引
├── docs/
│   ├── guide.md                # 详细使用指南
│   ├── parsing_rules.md        # 解析规则说明
│   └── troubleshooting.md      # 异常处理指南
├── assets/
│   ├── api_list.template.md    # 接口清单生成模板
│   └── function_table.template.md  # 函数表模板
├── examples/
│   └── qtee_api_sample.md      # 已处理示例（QTEE API）
└── evals/
    └── evals.json              # 测试用例（含异常场景）
```

## 边界与不做的事

1. **不处理非 API 文档**：硬件设计手册、产品规格书、应用笔记等不含 API 调用的文档
2. **不替代 az-pdf2md**：本技能依赖 az-pdf2md 完成 PDF→MD 转换
3. **不做代码生成**：仅整理接口清单，不生成示例代码（示例代码从原 PDF 提取）
4. **不修改原 PDF/原 .md 内容**：仅在末尾追加索引章节

## 相关文档

- 详细使用指南：[docs/guide.md](docs/guide.md)
- 解析规则：[docs/parsing_rules.md](docs/parsing_rules.md)
- 异常处理：[docs/troubleshooting.md](docs/troubleshooting.md)
- 前置技能：[az-pdf2md](../az-pdf2md/SKILL.md)
- 处理示例：[examples/qtee_api_sample.md](examples/qtee_api_sample.md)
