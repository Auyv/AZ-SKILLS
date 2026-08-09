---
name: az-api-summary
description: |
  当用户提供 CHM、PDF、HTML、Markdown 或 SDK 接口资料，并要求“总结 API 接口”“按分层架构整理接口”
  “列出有哪些 API”“生成接口说明 Markdown”“参考另一份文档的篇章格式”或“补充密码学术语表”时使用。
  解析接口资料，区分公共 API、内部协议接口和平台移植接口，输出带术语表、函数概览、调用流程、
  项目范围映射及限制说明的 Markdown 文档。
version: 1.0.0

inputs:
  required:
    - name: source_documents
      type: array[file]
      description: 一个或多个待总结的本地接口文档，支持 CHM、PDF、HTML、Markdown、DOCX
      validation: 至少一个文件；路径存在；文件可读；扩展名受支持
  optional:
    - name: reference_document
      type: file
      description: 只用于参考篇章结构和版式节奏的文档，不复制其内容
      default: null
    - name: output_path
      type: file
      description: 输出 Markdown 路径
      default: 主接口文档同目录下的“原文件名-分层架构总结.md”
    - name: architecture_model
      type: string
      description: 用户指定的架构层次；未指定时从目录、模块和源码路径推断
      default: auto
    - name: audience
      type: string
      description: 目标读者，例如密码学初学者、应用开发者、平台移植者
      default: 应用开发者和初学者
    - name: project_context
      type: string
      description: 项目能力边界、命名语义、允许或禁止开放的功能
      default: null
    - name: overwrite
      type: boolean
      description: 是否允许覆盖已有输出；覆盖前必须备份
      default: false

outputs:
  - name: result
    type: object
    description: 包含 status、timestamp、data 或 error 的标准响应
    format: JSON object
  - name: summary_md
    type: file
    description: 分层 API 总结 Markdown
    path: "{output_path}"
  - name: api_manifest
    type: file
    description: 可选的结构化 API 提取清单，存放在临时目录供校验使用
    path: ".ai_tmp/api-summary/{task_id}/api_manifest.json"

errors:
  - code: ERR_MISSING_INPUT
    message: 未提供接口源文档
    handling: 返回缺少 source_documents，不创建输出
  - code: ERR_TYPE_INVALID
    message: 输入字段类型错误
    handling: 说明字段期望类型和示例
  - code: ERR_FILE_NOT_FOUND
    message: 输入文件不存在
    handling: 保留原路径并请用户修正，不猜测相似文件
  - code: ERR_FORMAT_UNSUPPORTED
    message: 文档格式不受支持
    handling: 列出支持格式；建议先转换为 PDF、HTML 或 Markdown
  - code: ERR_CHM_EXTRACT_FAILED
    message: CHM 解包失败
    handling: 保留 hh.exe/7z 原始错误；尝试另一个可用解包器或请求用户提供解包目录
  - code: ERR_DEPENDENCY_MISSING
    message: 缺少 PDF 渲染或 HTML 解析依赖
    handling: 不自动安装；说明缺失工具和可用降级方法
  - code: ERR_OUTPUT_EXISTS
    message: 输出文件已存在且未允许覆盖
    handling: 停止写入；请求改名或显式允许覆盖
  - code: ERR_PERMISSION_DENIED
    message: 文件不可读或输出目录不可写
    handling: 返回具体路径和原始权限错误
  - code: ERR_API_EXTRACTION_EMPTY
    message: 未从资料中识别出接口
    handling: 检查编码、目录和文档类型；返回部分结构并要求人工确认
---

# API 接口分层总结

## 做什么

把分散在 CHM、PDF、HTML、头文件说明和示例文档中的接口信息整理成可导航的 Markdown：

- 按架构层次区分公共 API、命令封装、协议抽象、链路协议和硬件适配接口。
- 为每个公共模块生成“头文件、职责、函数概览、原型、关键参数、返回值和限制”。
- 保留内部接口索引，但明确标注业务是否应直接调用。
- 生成推荐调用流程、关键数据结构、错误模型和项目能力映射。
- 面向非密码学专业读者生成上下文术语表，避免只写英文缩写。
- 使用结构化清单反查 Markdown，确认 API 不漏项。

## 何时使用

满足任一条件时使用：

- 用户提供 `.chm`、`.pdf`、Doxygen HTML 或 SDK 接口资料并要求总结。
- 用户想快速知道“有哪些 API、各自做什么、属于哪层”。
- 用户要求按已有 PDF/手册的篇章格式输出新的接口说明。
- 用户要求为接口文档增加密码学、协议或硬件术语解释。

普通代码评审、单个函数解释、没有接口资料的架构设计不触发本技能。

## 怎么做

### 1. 读取规则和验证输入

1. 读取当前项目 `AGENTS.md` 和它引用的全局规则。
2. 完整读取本技能直接引用的模板或 FAQ 后再执行。
3. 运行 `scripts/validate_inputs.py`，或按同等规则人工验证。
4. 输出已确认的源文件、参考文件和目标路径。
5. 若输出已存在且 `overwrite=false`，返回 `ERR_OUTPUT_EXISTS`。
6. 若允许覆盖，先识别编码并备份到 `.ai_tmp/bak/`。

### 2. 解析资料

#### CHM

1. 在 Windows 上优先使用 `hh.exe -decompile <temp_dir> <source.chm>`；也可使用已存在的 7z。
2. 解包到 `.ai_tmp/api-summary/<task_id>/chm/`，不要修改原 CHM。
3. 识别 Doxygen 的 `modules.html`、`files.html`、`group_*.html`、`*_source.html`。
4. 使用 `scripts/extract_doxygen_api.py` 提取模块、函数名、原型和简述。
5. 同时扫描头文件源码页，补齐“有声明但没有详情块”的宏和内部函数。
6. 用 UTF-8 读取 HTML；若中文错显，先检测编码，不要直接把乱码写进总结。

#### PDF

1. 若 PDF 技能可用，必须遵循其文本提取和页面渲染流程。
2. 用 `pdfplumber`/`pypdf` 提取目录和正文。
3. 渲染目录、架构图、函数概览、API 详解、流程图和限制表等代表页面并视觉检查。
4. 不仅依赖文本提取，因为表格列和流程图可能错序。

#### 参考文档

只提炼其篇章组织方式，例如：

```text
适用范围 -> 架构/方案概述 -> 模块介绍 -> 函数概览 -> API 详解
         -> 数据结构/枚举 -> 推荐调用流程 -> 示例 -> 限制/备注
```

不要复制参考文档的技术内容、品牌元素或受版权保护的长段落。

### 3. 建立架构模型

优先使用源文档明确给出的层次。没有明确层次时，综合以下证据推断：

1. 模块树和父子关系。
2. 头文件/源码目录，例如 `api/`、`cmd/`、`proto/`、`hal/`、`port/`。
3. 函数前缀，例如 `api_*`、`apdu_*`、`tpdu_*`、`proto_*`、`port_*`。
4. 调用引用和回调结构。

每个接口必须标注以下类别之一：

- 业务可直接调用的公共 API。
- SDK 内部接口，仅用于理解或排障。
- 平台移植接口，需要在目标 OS/MCU 上实现。
- 文档存在但项目当前不开放的能力。

若分类是推断而非源文档明示，明确写“根据目录/调用关系推断”。

### 4. 生成 Markdown

以 `assets/api-summary-template.md` 为骨架。最低内容要求：

1. 文档信息、来源和适用范围。
2. **术语表**，放在正文前部。
3. 分层架构图和各层职责表。
4. 公共 API 统计及完整索引。
5. 每个公共模块的函数概览、原型和关键限制。
6. 内部层接口索引及调用关系。
7. 核心结构体、枚举和错误模型。
8. 推荐初始化及业务调用流程。
9. 项目能力映射，区分 SDK 全量能力与当前项目范围。
10. 安全、并发、长度、补位、缓冲区和重试限制。

### 5. 术语表要求

术语表是强制章节，不得因用户未主动要求而省略。至少覆盖实际正文出现的类别：

- 系统和架构：SE、Host、SDK、API、RPC、HAL、Port。
- 通信和协议：SPI、CS、RST、ATR、APDU、TPDU、CLA、INS、Lc、Le、SW。
- 密钥语义：公钥、私钥、固定密钥、会话密钥、shared key、key slot、`key_id`、业务 `session_id`。
- 密码功能：加密、解密、签名、验签、HASH、MAC、随机数、密钥协商。
- 算法：仅解释资料实际出现的 AES、3DES、RSA、ECC、SM2/3/4/9、SHA 等。
- 模式和编码：ECB、CBC、IV、NoPadding、PKCS#7、PKCS#1、ISO9797 M1/M2。
- 数据格式：如 `R||S`、`X||Y`、`C1||C3||C2`、ZA。

每个术语必须说明“在当前 SDK/项目里是什么意思”。对容易混淆的概念先给对照表：

- 加密不等于签名。
- HASH 不等于 MAC。
- `key_id` 不等于用户、进程或会话 ID。
- 会话密钥不等于业务 `session_id`。
- 身份认证不等于权限授权。

未知或厂商专用缩写不得强行扩展；写明“具体定义以产品规范为准”。

### 6. 校验

完成前执行以下检查：

1. 从结构化清单生成公共 API 唯一集合。
2. 检查每个公共 API 名称都出现在 Markdown 中。
3. 检查内部接口未被描述为业务直接调用。
4. 检查术语表包含正文中的高频缩写、算法、模式和 ID 语义。
5. 检查 Markdown 代码围栏成对、表格列数一致、目录锚点有效。
6. 用 UTF-8 解码并确认没有 U+FFFD 替换字符。
7. 抽查源文档中的危险限制，例如认证锁死、不可逆写入、长度上限和密钥泄露风险。

### 7. 输出

响应必须包含：

```json
{
  "status": "success | partial | error",
  "timestamp": "ISO-8601",
  "data": {
    "summary_path": "absolute path",
    "public_api_count": 0,
    "internal_api_count": 0,
    "warnings": []
  }
}
```

交付时提供可点击的绝对路径，并概括接口数量、架构层次和重要待确认项。

## 错误处理和降级

- CHM 无法解包：保留原始错误；若可打开 GUI，可请求用户导出 HTML；否则返回部分结果。
- Doxygen 分组页为空：扫描 `*_source.html` 和头文件声明补齐接口。
- PDF 表格文本错序：以渲染页面视觉结果为准，人工重建表格。
- 中文乱码：检测 UTF-8/GBK/GB2312 后重新读取，不在乱码基础上总结。
- 缺少解析依赖：不自动安装；使用现有工具或请求用户提供转换后的文档。
- 多份资料冲突：列出冲突和各自来源，不擅自选一个作为事实。
- 文档没有架构信息：按目录和前缀推断，并标记推断性质。

## 相关文档

- 输出模板：[assets/api-summary-template.md](assets/api-summary-template.md)
- 异常处理 FAQ：[docs/faq/faq-az-api-summary.md](docs/faq/faq-az-api-summary.md)
- 手动执行指南：[MANUAL.md](MANUAL.md)

## 技能文件结构

```text
az-api-summary/
├── SKILL.md
├── MANUAL.md
├── scripts/
│   ├── validate_inputs.py
│   └── extract_doxygen_api.py
├── assets/
│   └── api-summary-template.md
├── docs/faq/
│   └── faq-az-api-summary.md
└── evals/
    └── evals.json
```

