#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)
TEST_BIN=${TEST_BIN:-/tmp/test_mpu_upgrade_n725a}

cc -std=c99 -Wall -Wextra -Werror \
    -DHQ_CMAKE_BUILD_MODEL_TYPE=4 \
    -I"$SCRIPT_DIR/include" \
    -I"$APP_DIR/mpu_upgrade/common/include" \
    "$APP_DIR/mpu_upgrade/common/mpu_upgrade_neoway_n725a.c" \
    "$SCRIPT_DIR/test_mpu_upgrade_n725a.c" \
    -o "$TEST_BIN"

"$TEST_BIN"
