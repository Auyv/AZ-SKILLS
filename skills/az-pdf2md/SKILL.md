---
name: az-pdf2md
description: |
  触发场景（满足任一即使用）：
  - 用户提到"PDF转MD"、"PDF转Markdown"、"转换PDF"
  - 用户提供了PDF文件路径并要求转换
  - 用户提到"SDK文档转换"、"API文档转换"
  - 用户要求批量处理PDF文件
  职责：将PDF文件（单个或批量）转换为结构完整、大模型友好的Markdown文档
  边界：仅处理本地PDF文件，不使用在线转换工具，不修改原始PDF
---

# PDF转大模型友好Markdown

## 做什么

接收用户提供的PDF文件路径（单个或多个，逗号分隔），将PDF转换为结构清晰、格式规范的Markdown文档。

**核心能力**：
- 支持可复制PDF和扫描件PDF（OCR识别）
- 自动清理页码、水印、页眉页脚等冗余内容
- 智能识别标题层级、代码块、参数表格
- 输出UTF-8编码的.md文件，与PDF同名、同目录

**工具选型**：pdfplumber（主，已安装轻量）+ MinerU（可选，技术文档还原度更高）

## 何时使用

1. 用户提供PDF文件路径并要求转为Markdown
2. 用户需要批量转换多个PDF文件
3. 用户需要将SDK/API技术文档转为大模型可读格式
4. 用户提到"PDF转MD"、"转换文档"等关键词

## 怎么做

### 第一步：环境检查

运行依赖检测脚本，确保 pdfplumber 等核心依赖已安装：

```python
python scripts/install_deps.py
```

如需MinerU高级模式（含OCR），加 `--heavy` 参数安装PyTorch等重型依赖。

### 第二步：路径验证

接收用户输入的PDF路径（逗号分隔），验证文件存在性和格式：

```python
python scripts/validate_paths.py "file1.pdf, file2.pdf"
```

- 自动去除路径两端的引号和空格
- 验证文件存在、扩展名为.pdf、文件头为PDF格式
- 无效路径会报告具体错误原因

### 第三步：执行转换

运行主编排脚本，自动完成转换+优化：

```python
python scripts/main.py "file1.pdf, file2.pdf"
```

**可选参数**：
- `--mode auto|pdfplumber|mineru` — 转换模式（默认auto）
- `--heavy` — 安装MinerU等重型依赖

**执行流程**：
1. 检查并安装缺失依赖
2. 验证所有PDF路径
3. 逐个PDF执行转换（pdfplumber优先，失败则尝试MinerU）
4. 对生成的MD进行后处理优化（清理冗余、修复表格、包裹代码块）
5. 输出转换结果摘要
6. 优化MD文件内容结构，确保人类和大模型都能高效理解

### 第四步：结果反馈

转换完成后，输出以下信息：

| 信息 | 说明 |
|------|------|
| 成功文件路径 | 每个MD文件的完整路径 |
| 成功/失败数量 | 统计摘要 |
| 失败原因 | 具体错误说明（如有） |

### 错误处理

| 场景 | 处理方式 |
|------|----------|
| 路径不存在 | 提示用户修正路径 |
| 非PDF文件 | 跳过并报告 |
| 加密PDF | 提示用户解除加密后重试 |
| 转换失败 | 自动尝试备选转换器 |
| 依赖缺失 | 提示安装命令 |

### 技能文件结构

```
az-sdk-pdf2md/
├── SKILL.md       (必需. 给AI Agent看的技能说明文档)
├── MANUAL.md      (必需. 给手动用户看的操作指南文档)
├── scripts/       - 可执行Python脚本
│   ├── install_deps.py      # 依赖检测与安装
│   ├── validate_paths.py    # 路径解析与验证
│   ├── convert_pdf.py       # 核心PDF转换
│   ├── optimize_md.py       # MD后处理优化
│   └── main.py              # 主编排器
├── docs/          - 文档
│   └── troubleshooting.md   # 常见问题排查
├── assets/        - 资源
│   └── md_cleanup_patterns.json  # MD清理规则
└── evals/         - 测试用例
    └── evals.json
```
