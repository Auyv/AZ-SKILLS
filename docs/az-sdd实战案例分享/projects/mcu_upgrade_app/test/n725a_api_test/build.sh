#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
NEOWAY_APP_SDK_PATH=${NEOWAY_APP_SDK_PATH:-/opt/neoway-arm-oe-linux}
CROSS_CC=${CROSS_CC:-"$NEOWAY_APP_SDK_PATH/sysroots/bin/arm-openwrt-linux-gcc"}
OUTPUT_DIR=${OUTPUT_DIR:-"$SCRIPT_DIR/build"}
BUILD_TYPE=${1:-Debug}

case "$BUILD_TYPE" in
    Debug)
        BUILD_FLAGS="-O0 -g3"
        ;;
    Release)
        BUILD_FLAGS="-O2 -DNDEBUG"
        ;;
    *)
        echo "Usage: $0 [Debug|Release]" >&2
        exit 2
        ;;
esac

if [ ! -x "$CROSS_CC" ]; then
    echo "Cross compiler not found: $CROSS_CC" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
export STAGING_DIR=${STAGING_DIR:-"$NEOWAY_APP_SDK_PATH/sysroots"}

# shellcheck disable=SC2086
"$CROSS_CC" -std=c99 -Wall -Wextra -Werror $BUILD_FLAGS \
    -I"$NEOWAY_APP_SDK_PATH/include" \
    "$SCRIPT_DIR/n725a_fota_api_test.c" \
    -L"$NEOWAY_APP_SDK_PATH/libs" \
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
    -o "$OUTPUT_DIR/n725a_fota_api_test"

"$NEOWAY_APP_SDK_PATH/sysroots/bin/arm-openwrt-linux-readelf" \
    -h "$OUTPUT_DIR/n725a_fota_api_test"

echo "Built: $OUTPUT_DIR/n725a_fota_api_test"
