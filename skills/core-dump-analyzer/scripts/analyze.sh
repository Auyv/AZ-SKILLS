#!/bin/bash
# Core Dump 分析脚本
# 优先级: WSL/Linux > Docker
# 用法: ./analyze.sh <core_file> <executable> <lib_dir> [symbol_dir]

set -e

CORE_FILE="$1"
EXECUTABLE="$2"
LIB_DIR="$3"
SYMBOL_DIR="${4:-}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 检查参数
if [ -z "$CORE_FILE" ] || [ -z "$EXECUTABLE" ] || [ -z "$LIB_DIR" ]; then
    echo "用法: $0 <core_file> <executable> <lib_dir> [symbol_dir]"
    exit 1
fi

# 检查文件存在
if [ ! -f "$CORE_FILE" ]; then
    log_error "Core dump 文件不存在: $CORE_FILE"
    exit 1
fi

if [ ! -f "$EXECUTABLE" ]; then
    log_error "可执行文件不存在: $EXECUTABLE"
    exit 1
fi

log_info "开始分析 Core Dump..."
log_info "Core 文件: $CORE_FILE"
log_info "可执行文件: $EXECUTABLE"
log_info "库目录: $LIB_DIR"

# 构建 solib-search-path
SOLIB_PATH="$LIB_DIR"
if [ -d "$SYMBOL_DIR" ]; then
    SOLIB_PATH="$SOLIB_PATH:$SYMBOL_DIR"
fi

# 查找可用的 GDB
find_gdb() {
    # 优先检查 WSL/Linux 系统中的 ARM GDB
    for gdb in arm-linux-gnueabihf-gdb arm-none-eabi-gdb arm-openwrt-linux-gnueabi-gdb gdb-multiarch; do
        if command -v $gdb &> /dev/null; then
            echo "$gdb"
            return 0
        fi
    done
    return 1
}

GDB_CMD=$(find_gdb)
if [ -z "$GDB_CMD" ]; then
    log_warn "未找到系统 ARM GDB，尝试使用 Docker..."
    # Docker 方式
    if command -v docker &> /dev/null; then
        log_info "使用 Docker 镜像进行分析..."
        docker run --rm \
            -v "$(dirname $CORE_FILE):/workspace" \
            -w /workspace \
            --entrypoint "bash" \
            ea2300410535 \
            -c "cd /workspace && \
                /opt/ql_crosstools/ql-ag35-1806e-gcc-13.3.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gnueabi-gdb \
                -batch \
                -ex 'file $(basename $EXECUTABLE)' \
                -ex 'set solib-search-path $LIB_DIR' \
                -ex 'core-file $(basename $CORE_FILE)' \
                -ex 'bt full' \
                -ex 'info registers' \
                -ex 'info sharedlibrary' \
                -ex 'x/10i \$pc'"
    else
        log_error "Docker 也不可用，请手动安装 ARM GDB 或使用 WSL"
        exit 1
    fi
else
    log_info "使用 GDB: $GDB_CMD"

    # WSL/Linux 方式
    $GDB_CMD -batch \
        -ex "file $EXECUTABLE" \
        -ex "set solib-search-path $SOLIB_PATH" \
        -ex "core-file $CORE_FILE" \
        -ex "echo \n=== REGISTERS ===\n" \
        -ex "info registers" \
        -ex "echo \n=== BACKTRACE ===\n" \
        -ex "bt full" \
        -ex "echo \n=== SHARED LIBRARIES ===\n" \
        -ex "info sharedlibrary" \
        -ex "echo \n=== DISASSEMBLE AT PC ===\n" \
        -ex "x/10i \$pc"
fi

log_info "分析完成"
