---
name: core-dump-analyzer
description: 自动分析 ARM Linux core dump，定位段错误原因，生成分析报告
version: 1.0.0
triggers:
  - "分析core dump"
  - "分析段错误"
  - "core dump分析"
  - "segmentation fault分析"
  - "coredump分析"
inputs:
  required:
    - coredump_path: "Core dump 文件路径（ELF 格式）"
    - executable_path: "可执行文件路径（与 core dump 对应的同一版本）"
    - so_lib_path: "SO库目录路径（可执行文件依赖的动态库）"
  optional:
    - dbg_symbol_path: "调试符号文件目录路径（.dbg 文件，可选）"
    - gdb_path: "ARM GDB 路径（可选，默认自动检测）"
outputs:
  - analysis_report: "Markdown 格式的分析报告"
---

# Core Dump 分析技能

## 技能目标

自动分析 ARM Linux core dump 文件，定位段错误（SIGSEGV）发生的根本原因，并生成详细的分析报告。

## 何时使用

用户发送以下内容时触发：
- "分析这个 core dump"
- "帮我看看这个段错误"
- "core dump 分析"
- 或者用户直接提供了 `.core` 文件路径

## 工作流程

### 步骤 1：检查必要文件

检查以下文件是否存在：
1. **Core dump 文件**（.core 文件）
2. **可执行文件**（ELF 格式，ARM 架构）
3. **SO 库文件**（.so 文件）
4. **调试符号文件**（.dbg 文件，可选但推荐）

如果文件缺失，向用户提示需要补充哪些文件。

### 步骤 2：确定分析环境

按优先级尝试以下环境：

1. **WSL/Linux** - 直接在 WSL 或 Linux 上使用 ARM GDB
2. **Docker** - 使用 `ea2300410535` 镜像中的工具链
3. **手动指南** - 提供命令让用户自行分析

### 步骤 3：执行 GDB 分析

使用 ARM GDB 加载 core dump，执行以下命令：
- `bt full` - 获取完整调用栈
- `info registers` - 获取寄存器状态
- `info sharedlibrary` - 查看加载的库
- `info symbol <address>` - 查询符号信息
- `x/10i $pc` - 查看崩溃处指令

### 步骤 4：代码审查

根据调用栈信息：
1. 定位崩溃发生的源文件
2. 找到崩溃时的函数调用链
3. 分析可能的根本原因

### 步骤 5：生成报告

生成 Markdown 格式的分析报告，包含：
- 崩溃基本信息（信号、地址、架构）
- 调用栈详情
- 寄存器状态
- 可疑代码分析
- 建议的修复方向

## 文件结构

```
core-dump-analyzer/
├── SKILL.md           # 本文件
├── scripts/
│   └── analyze.sh     # Docker 分析脚本
├── docs/
│   └── guide.md       # 使用指南
└── assets/
    └── template.md    # 报告模板
```

## 注意事项

- 仅支持 ARM 架构的 core dump 分析
- 需要 Docker 环境运行分析容器
- 如果调试符号缺失，调用栈可能无法完全解析
- 分析结果基于静态代码审查和 core dump 信息，实际情况可能需要设备调试验证

## 错误处理

- Docker 不可用时，提示用户使用 WSL 或 Linux 环境
- 缺少调试符号时，提供尽可能多的信息并说明限制
- 分析失败时，生成部分报告并说明原因
