#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)
TASK_MPU_DIR=$(CDPATH= cd -- "$APP_DIR/../.." && pwd)
PROJECT_SDK_PATH=${PROJECT_SDK_PATH:-"$TASK_MPU_DIR/sdk"}
NEOWAY_APP_SDK_PATH=${NEOWAY_APP_SDK_PATH:-/opt/neoway-arm-oe-linux}
CROSS_CC=${CROSS_CC:-"$NEOWAY_APP_SDK_PATH/sysroots/bin/arm-openwrt-linux-gcc"}
OUTPUT_FILE=${OUTPUT_FILE:-/tmp/n725a_mpu_upgrade_link_smoke}

if [ -d "$PROJECT_SDK_PATH/n725a/include" ]; then
    PROJECT_SDK_PATH="$PROJECT_SDK_PATH/n725a"
fi

export STAGING_DIR=${STAGING_DIR:-"$NEOWAY_APP_SDK_PATH/sysroots"}

"$CROSS_CC" -std=c99 -Wall -Wextra -Werror \
    -DHQ_CMAKE_BUILD_MODEL_TYPE=4 \
    -I"$APP_DIR/mpu_upgrade/common/include" \
    -I"$NEOWAY_APP_SDK_PATH/include" \
    -I"$PROJECT_SDK_PATH/include" \
    "$APP_DIR/mpu_upgrade/common/mpu_upgrade_neoway_n725a.c" \
    "$SCRIPT_DIR/link_smoke_main.c" \
    -L"$PROJECT_SDK_PATH/lib/debug" \
    -L"$NEOWAY_APP_SDK_PATH/libs" \
    -lalog \
    -lsfc \
    -lnwy_service \
    -lnwy_common \
    -ldl \
    -lpthread \
    -lm \
    -lmtel \
    -lrilutil \
    -llog \
    -lprop2uci \
    -luci \
    -lubox \
    -lubus \
    -lblobmsg_json \
    -ljson_script \
    -ljson-c \
    -o "$OUTPUT_FILE"

"$NEOWAY_APP_SDK_PATH/sysroots/bin/arm-openwrt-linux-readelf" \
    -h "$OUTPUT_FILE"
