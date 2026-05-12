# Core Dump 分析技能使用指南

## 快速开始

当遇到段错误时，直接提供以下信息即可开始分析：

```
帮我分析 core dump
- core 文件: /path/to/xxx.core
- 可执行文件: /path/to/ivi_app
- so 库目录: /path/to/lib/release
```

## 必要的文件

### 1. Core Dump 文件

从设备拉取的 core dump 文件，文件名格式不固定，例如：
- `core.xxx`
- `core.xxx.app_name`
- `xxx.core`
- `app_name.core.time.pid`

> 注意：文件名不是固定的，关键是 ELF 格式的 core dump 文件

### 2. 可执行文件

与 core dump 对应的可执行文件：
- 必须与 core dump 是同一版本编译
- 通常位于 `/vendor/app/hq/bin/` 或类似路径
- 例如：`ivi_app`、`my_service`、`app_daemon` 等（不是固定名称）

### 3. SO 库文件

可执行文件依赖的动态链接库：
- 通常位于 `/vendor/app/hq/lib/` 或类似路径
- 需要包含所有 `.so` 文件

### 4. 调试符号文件（可选但推荐）

用于解析函数名的调试符号：
- 通常是 `.dbg` 后缀的文件
- 如 `your_app.dbg`、`libxxx.so.dbg`
- 与可执行文件一一对应

## 分析环境优先级

技能按以下优先级使用分析环境：

1. **WSL / Linux** - 直接使用系统中的 ARM GDB
2. **Docker** - 使用 `ea2300410535` 镜像中的工具链
3. **手动指南** - 提供命令让用户自行分析

### WSL 中安装 ARM GDB

```bash
sudo apt update
sudo apt install gdb-multiarch
# 或
sudo apt install gcc-arm-linux-gnueabihf gdb-arm-none-eabi
```

## 分析流程

1. **文件检查** - 验证所有必要文件存在
2. **Docker 分析** - 使用 ARM GDB 提取 core dump 信息
3. **代码审查** - 根据调用栈分析源代码
4. **报告生成** - 输出 Markdown 格式分析报告

## 报告输出位置

分析报告默认保存到：
```
当前目录/CORE_DUMP_ANALYSIS_REPORT.md
```

## 常见问题

### Q: 没有调试符号怎么办？
A: 可以进行部分分析，但调用栈中的函数名可能显示为地址。可以尝试手动使用 `addr2line` 工具将地址转换为源码位置。

### Q: Docker 不可用怎么办？
A: 可以使用 WSL 或 Linux 环境，按照报告中提供的 GDB 命令手动分析。

### Q: 报告保存位置？
A: 报告默认保存到用户提供的 core dump 所在目录，或当前工作目录。

## 示例

### 输入
```
帮我分析这个 core dump，文件在:
- /tmp/1777452508.ivi_app.11.core
- 可执行文件在: /tmp/ivi_app
- 库在: /tmp/lib/release
```

### 输出
技能会生成完整的分析报告，包含：
- 崩溃调用栈
- 寄存器状态
- 可疑代码位置
- 修复建议
