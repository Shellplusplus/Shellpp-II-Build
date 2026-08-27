#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_PROJECT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
NATIVE_PROJECT=${SHELLPP_NATIVE_DIR:-/Users/ikun_cxkpro/Projects/Shell++/Shellpp-ii}
TARGET_ID=xiaomi-band-10-pro-3.101.043
PATCH_DIR="$BUILD_PROJECT/targets/$TARGET_ID/patches"
WORK_DIR="$SCRIPT_DIR/.build-ui"
TARGET_SOURCE_DIR="$WORK_DIR/target-src"
CC_BIN=${CC:-/usr/bin/clang}

if [ ! -x "$CC_BIN" ]; then
    echo "host C compiler is not executable: $CC_BIN" >&2
    exit 1
fi
if [ ! -d "$NATIVE_PROJECT/module/src" ] ||
        [ ! -d "$NATIVE_PROJECT/module/include" ]; then
    echo "nativeApp source is missing: $NATIVE_PROJECT" >&2
    exit 1
fi
if [ ! -d "$PATCH_DIR" ]; then
    echo "043 patch directory is missing: $PATCH_DIR" >&2
    exit 1
fi

mkdir -p "$TARGET_SOURCE_DIR"
cp "$NATIVE_PROJECT/module/src/"*.c "$NATIVE_PROJECT/module/src/"*.S \
    "$TARGET_SOURCE_DIR/"
for patch_file in "$PATCH_DIR"/*.patch; do
    patch -s -d "$TARGET_SOURCE_DIR" -p1 < "$patch_file"
done

# The two reboot functions are never invoked by this host test. Replace their
# ARM-only wait instruction in the temporary replay tree so native Clang can
# assemble the same UI logic for ASan/UBSan execution.
sed -i.bak 's/__asm__ volatile("wfi")/(void)0/g' \
    "$TARGET_SOURCE_DIR/native_ui.c"

"$CC_BIN" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -Wno-unused-function -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$SCRIPT_DIR/include" -I "$NATIVE_PROJECT/module/include" \
    "$TARGET_SOURCE_DIR/native_ui.c" "$SCRIPT_DIR/native_ui_host_test.c" \
    -o "$WORK_DIR/native_ui_host_test"
"$WORK_DIR/native_ui_host_test"
