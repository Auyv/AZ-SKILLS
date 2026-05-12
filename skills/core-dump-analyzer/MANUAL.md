# Core Dump 手动分析指南

如果 AI Agent 不可用，可以手动执行以下步骤来分析 core dump。

## 前提条件

- Linux 或 WSL 环境
- ARM 交叉编译工具链（arm-linux-gnueabihf-gdb 或类似工具）
- Docker（可选）

## 方法一：使用 WSL / Linux + ARM GDB（推荐）

### 1. 安装 ARM GDB

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install gdb-multiarch
# 或安装完整工具链
sudo apt install gcc-arm-linux-gnueabihf gdb-arm-none-eabi
```

**其他发行版:**
```bash
sudo yum install arm-none-eabi-gdb
# 或
sudo dnf install arm-none-eabi-gdb
```

### 2. 准备文件

将以下文件放到同一目录（文件名仅为示例，请根据实际情况提供）：
- `xxx.core` - core dump 文件
- `your_app` - 可执行文件（你的应用名称）
- `your_app.dbg` - 调试符号文件（可选）
- `*.so` - 所有依赖的动态库

### 3. 执行 GDB 分析

```bash
# 如果有 ARM GDB
arm-linux-gnueabihf-gdb -batch \
  -ex "file ivi_app" \
  -ex "set solib-search-path /path/to/libs" \
  -ex "core-file xxx.core" \
  -ex "bt full" \
  -ex "info registers"

# 或使用 gdb-multiarch
gdb-multiarch ivi_app -batch \
  -ex "set architecture arm" \
  -ex "set solib-search-path /path/to/libs" \
  -ex "core-file xxx.core" \
  -ex "bt full"
```

## 方法二：使用 Docker

### 1. 启动 Docker 容器

```bash
docker run --rm \
  -v /your/path:/workspace \
  -w /workspace \
  ea2300410535 \
  bash
```

### 2. 执行 GDB 分析

```bash
cd /workspace

/opt/ql_crosstools/ql-ag35-1806e-gcc-13.3.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gnueabi-gdb \
  -batch \
  -ex "file your_app" \
  -ex "symbol-file your_app.dbg" \
  -ex "set solib-search-path /workspace/lib/release:/workspace/lib/debug" \
  -ex "core-file xxx.core" \
  -ex "bt full" \
  -ex "info registers" \
  -ex "info sharedlibrary" \
  -ex "x/10i \$pc"
```

## 关键 GDB 命令

| 命令 | 说明 |
|------|------|
| `bt` | 显示调用栈 |
| `bt full` | 显示完整调用栈（含局部变量） |
| `info registers` | 显示寄存器状态 |
| `info sharedlibrary` | 显示加载的共享库 |
| `x/10i \$pc` | 显示崩溃处指令 |
| `info symbol <addr>` | 查询地址对应的符号 |
| `thread apply all bt` | 显示所有线程的调用栈 |

## 报告模板

按照 `assets/template.md` 中的格式整理分析结果。

## 常见问题排查

### 问题：can't find file
- 检查可执行文件路径是否正确
- 检查 core dump 文件是否损坏

### 问题：no symbol table
- 需要对应的 .dbg 文件
- 或使用 strip 前的未脱符号版本

### 问题：can't read memory
- core dump 可能不完整
- 尝试在设备上重新生成
