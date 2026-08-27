#!/bin/sh
# Build Shell++ II firmware profiles and deploy their compiler-managed bins.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=/Users/ikun_cxkpro/Projects/Shell++/Shellpp-ii
SOURCE_DIR="$PROJECT_DIR/module"
TARGET_DIR="$SCRIPT_DIR/targets"
INSTALLER_DIR=/Users/ikun_cxkpro/Projects/Shell++/Shellpp-ii-installer
INSTALLER_LUA_DIR="$INSTALLER_DIR/_Lua"
INSTALLER_PACKAGED_DIR="$INSTALLER_DIR/resources/_lua/_Lua"
REPACK_SCRIPT="$SCRIPT_DIR/repack_resource.py"

CLANG_BIN=${CLANG:-/usr/bin/clang}
RUST_TOOLCHAIN_DIR=/Users/ikun_cxkpro/.rustup/toolchains/stable-x86_64-apple-darwin
if [ -n "${RUST_LLD:-}" ]; then
    LLD_BIN=$RUST_LLD
    LLD_LIBRARY_DIR=${RUST_LLD_LIBRARY_PATH:-}
else
    LLD_BIN=$RUST_TOOLCHAIN_DIR/lib/rustlib/x86_64-apple-darwin/bin/rust-lld
    LLD_LIBRARY_DIR=$RUST_TOOLCHAIN_DIR/lib
fi
PYTHON_BIN=${PYTHON:-/usr/local/bin/python3}
BUILD_TARGET=

while [ "$#" -gt 0 ]; do
    case "$1" in
        --target)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "--target requires a target id" >&2
                exit 2
            fi
            BUILD_TARGET=$2
            shift 2
            ;;
        *)
            echo "unknown argument: $1" >&2
            echo "usage: $0 [--target TARGET_ID]" >&2
            exit 2
            ;;
    esac
done

require_executable() {
    if [ ! -x "$1" ]; then
        echo "$2 is not executable: $1" >&2
        exit 1
    fi
}

require_executable "$CLANG_BIN" clang
require_executable "$LLD_BIN" rust-lld
require_executable "$PYTHON_BIN" python
if [ -n "$LLD_LIBRARY_DIR" ] && [ ! -d "$LLD_LIBRARY_DIR" ]; then
    echo "rust-lld library directory is missing: $LLD_LIBRARY_DIR" >&2
    exit 1
fi

run_lld() {
    if [ -n "$LLD_LIBRARY_DIR" ]; then
        env DYLD_LIBRARY_PATH="$LLD_LIBRARY_DIR" "$LLD_BIN" "$@"
    else
        "$LLD_BIN" "$@"
    fi
}

if [ ! -d "$SOURCE_DIR/src" ] || [ ! -d "$SOURCE_DIR/include" ]; then
    echo "nativeApp source is missing: $SOURCE_DIR" >&2
    exit 1
fi
if [ ! -d "$TARGET_DIR" ]; then
    echo "target profile directory is missing: $TARGET_DIR" >&2
    exit 1
fi
for installer_lua_dir in "$INSTALLER_LUA_DIR" "$INSTALLER_PACKAGED_DIR"; do
    if [ ! -d "$installer_lua_dir" ] \
        || [ ! -f "$installer_lua_dir/main.lua" ] \
        || [ ! -f "$installer_lua_dir/shellpp_ii_icon.bin" ]; then
        echo "installer Lua directory is incomplete: $installer_lua_dir" >&2
        exit 1
    fi
done
if [ ! -f "$REPACK_SCRIPT" ]; then
    echo "resource repack script is missing: $REPACK_SCRIPT" >&2
    exit 1
fi
for shared_resource in main.lua shellpp_ii_icon.bin; do
    if ! cmp -s \
        "$INSTALLER_LUA_DIR/$shared_resource" \
        "$INSTALLER_PACKAGED_DIR/$shared_resource"; then
        echo "installer shared resource differs between editing and packaged directories: $shared_resource" >&2
        exit 1
    fi
done

sha256_of() {
    shasum -a 256 "$1" | awk '{print $1}'
}

if [ -n "$BUILD_TARGET" ]; then
    case "$BUILD_TARGET" in
        *[!A-Za-z0-9._-]*)
            echo "invalid target id: $BUILD_TARGET" >&2
            exit 2
            ;;
    esac
    PROFILES="$TARGET_DIR/$BUILD_TARGET.env"
    if [ ! -f "$PROFILES" ]; then
        echo "target profile not found: $PROFILES" >&2
        exit 2
    fi
    DEPLOY_STAGE="$SCRIPT_DIR/out/.installer-stage-$BUILD_TARGET"
else
    PROFILES="$TARGET_DIR"/*.env
    DEPLOY_STAGE="$SCRIPT_DIR/out/.installer-stage"
fi

profile_count=0
for profile in $PROFILES; do
    if [ ! -f "$profile" ]; then
        echo "no target profiles found in $TARGET_DIR" >&2
        exit 1
    fi
    profile_count=$((profile_count + 1))
done

rm -rf "$DEPLOY_STAGE"
mkdir -p "$DEPLOY_STAGE"

build_target() {
    profile=$1
    profile_name=$(basename "$profile" .env)
    staged_abi="$DEPLOY_STAGE/.$profile_name.shellpp_target_abi.h"

    # Validate the complete profile before letting the shell read its plain
    # assignments. This rejects unknown keys and shell syntax first.
    "$PYTHON_BIN" "$SCRIPT_DIR/generate_target_abi.py" "$profile" "$staged_abi"

    unset TARGET_ID FIRMWARE_VERSION FIRMWARE_CODE FIRMWARE_IMAGE
    unset FIRMWARE_IMAGE_SIZE FIRMWARE_IMAGE_SHA256 CPU FLOAT_ABI
    unset MAX_LOADED_SIZE MAX_BSS_SIZE
    # Profiles are maintained here and intentionally use plain KEY=VALUE
    # assignments so the target configuration remains inspectable.
    . "$profile"

    OUT_DIR="$SCRIPT_DIR/out/$TARGET_ID"
    GENERATED_DIR="$OUT_DIR/generated"
    ABI_HEADER="$GENERATED_DIR/shellpp_target_abi.h"
    MODULE_NAME="shellpp_ii-$FIRMWARE_VERSION.bin"
    MODULE_PATH="$OUT_DIR/$MODULE_NAME"
    TARGET_SOURCE_DIR="$SOURCE_DIR/src"
    TARGET_PATCH_DIR="$TARGET_DIR/$TARGET_ID/patches"

    mkdir -p "$GENERATED_DIR"
    cp "$staged_abi" "$ABI_HEADER"
    cmp -s "$staged_abi" "$ABI_HEADER"

    if [ ! -f "$FIRMWARE_IMAGE" ]; then
        echo "firmware image is missing for $TARGET_ID: $FIRMWARE_IMAGE" >&2
        exit 1
    fi
    actual_size=$(wc -c < "$FIRMWARE_IMAGE" | tr -d '[:space:]')
    if [ "$actual_size" != "$FIRMWARE_IMAGE_SIZE" ]; then
        echo "firmware size mismatch for $TARGET_ID: expected $FIRMWARE_IMAGE_SIZE, got $actual_size" >&2
        exit 1
    fi
    actual_sha256=$(sha256_of "$FIRMWARE_IMAGE")
    if [ "$actual_sha256" != "$FIRMWARE_IMAGE_SHA256" ]; then
        echo "firmware SHA-256 mismatch for $TARGET_ID" >&2
        echo "expected: $FIRMWARE_IMAGE_SHA256" >&2
        echo "actual:   $actual_sha256" >&2
        exit 1
    fi

    mkdir -p "$OUT_DIR"
    rm -f "$OUT_DIR"/*.o "$OUT_DIR"/shellpp-ii-icon.bmp
    rm -f "$OUT_DIR"/shellpp_ii*.bin

    if [ -d "$TARGET_PATCH_DIR" ]; then
        TARGET_SOURCE_DIR="$OUT_DIR/target-src"
        rm -rf "$TARGET_SOURCE_DIR"
        mkdir -p "$TARGET_SOURCE_DIR"
        cp "$SOURCE_DIR"/src/*.c "$SOURCE_DIR"/src/*.S "$TARGET_SOURCE_DIR/"

        patch_count=0
        for patch_file in "$TARGET_PATCH_DIR"/*.patch; do
            if [ ! -f "$patch_file" ]; then
                echo "target patch directory contains no patch files: $TARGET_PATCH_DIR" >&2
                exit 1
            fi
            patch_count=$((patch_count + 1))
            patch -s -d "$TARGET_SOURCE_DIR" -p1 < "$patch_file"
        done
        echo "Applied $patch_count source patch(es) for $TARGET_ID"
    fi

    compile_source() {
        source_path=$1
        object_path=$2
        "$CLANG_BIN" \
            --target=arm-none-eabi \
            -mcpu="$CPU" \
            -mthumb \
            -mfloat-abi="$FLOAT_ABI" \
            -Oz \
            -ffreestanding \
            -fno-builtin \
            -fno-common \
            -fno-stack-protector \
            -fno-unwind-tables \
            -fno-asynchronous-unwind-tables \
            -fno-exceptions \
            -fomit-frame-pointer \
            -mlong-calls \
            -Wall \
            -Wextra \
            -Werror \
            -I "$GENERATED_DIR" \
            -I "$SOURCE_DIR/include" \
            -c "$source_path" \
            -o "$object_path"
    }

    compile_source "$TARGET_SOURCE_DIR/supervisor.c" "$OUT_DIR/supervisor.o"
    compile_source "$TARGET_SOURCE_DIR/native_app.c" "$OUT_DIR/native_app.o"
    compile_source "$TARGET_SOURCE_DIR/native_fs.c" "$OUT_DIR/native_fs.o"
    compile_source "$TARGET_SOURCE_DIR/native_ui.c" "$OUT_DIR/native_ui.o"
    compile_source "$TARGET_SOURCE_DIR/module_prelude.S" "$OUT_DIR/module_prelude.o"

    run_lld \
        -flavor gnu \
        -m armelf \
        -r \
        -T "$SCRIPT_DIR/shellpp_ii.ld" \
        -u module_initialize \
        -o "$MODULE_PATH" \
        "$OUT_DIR/module_prelude.o" \
        "$OUT_DIR/supervisor.o" \
        "$OUT_DIR/native_app.o" \
        "$OUT_DIR/native_fs.o" \
        "$OUT_DIR/native_ui.o"

    "$PYTHON_BIN" "$SCRIPT_DIR/verify_shellpp_elf.py" \
        --abi-header "$ABI_HEADER" \
        --max-loaded-size "$MAX_LOADED_SIZE" \
        --max-bss-size "$MAX_BSS_SIZE" \
        "$MODULE_PATH"

    cp "$MODULE_PATH" "$DEPLOY_STAGE/$MODULE_NAME"
    cmp -s "$MODULE_PATH" "$DEPLOY_STAGE/$MODULE_NAME"
    echo "Built $MODULE_PATH"
}

for profile in $PROFILES; do
    build_target "$profile"
done

staged_count=$(find "$DEPLOY_STAGE" -mindepth 1 -maxdepth 1 -type f -name 'shellpp_ii-*.bin' | wc -l | tr -d '[:space:]')
if [ "$staged_count" != "$profile_count" ]; then
    echo "staged module count mismatch: expected $profile_count, got $staged_count" >&2
    exit 1
fi

# All selected targets passed. A full build replaces the complete managed bin
# set; a single-target build replaces only that target and preserves all other
# firmware bins byte-for-byte. Both installer resource trees are kept in sync,
# then resource.bin and hashCode are rebuilt from the packaged tree.
for installer_lua_dir in "$INSTALLER_LUA_DIR" "$INSTALLER_PACKAGED_DIR"; do
    if [ -z "$BUILD_TARGET" ]; then
        find "$installer_lua_dir" -mindepth 1 -maxdepth 1 -type f \
            -name 'shellpp_ii-*.bin' -delete
        rm -f "$installer_lua_dir/shellpp_ii.bin"
    fi
    for module in "$DEPLOY_STAGE"/shellpp_ii-*.bin; do
        cp "$module" "$installer_lua_dir/$(basename "$module")"
    done
done

for module in "$DEPLOY_STAGE"/shellpp_ii-*.bin; do
    module_name=$(basename "$module")
    for installer_lua_dir in "$INSTALLER_LUA_DIR" "$INSTALLER_PACKAGED_DIR"; do
        deployed="$installer_lua_dir/$module_name"
        cmp -s "$module" "$deployed" || {
            echo "deployed module differs from build output: $deployed" >&2
            exit 1
        }
    done
done

for shared_resource in main.lua shellpp_ii_icon.bin; do
    cmp -s \
        "$INSTALLER_LUA_DIR/$shared_resource" \
        "$INSTALLER_PACKAGED_DIR/$shared_resource" || {
        echo "installer shared resource changed during deployment: $shared_resource" >&2
        exit 1
    }
done

"$PYTHON_BIN" "$REPACK_SCRIPT" --project "$INSTALLER_DIR"

if [ -n "$BUILD_TARGET" ]; then
    echo "Deployed only $BUILD_TARGET to both installer Lua directories"
    shasum -a 256 "$DEPLOY_STAGE"/shellpp_ii-*.bin \
        "$INSTALLER_LUA_DIR"/shellpp_ii-*.bin \
        "$INSTALLER_PACKAGED_DIR"/shellpp_ii-*.bin \
        "$INSTALLER_DIR/resource.bin" "$INSTALLER_DIR/hashCode"
else
    echo "Deployed $profile_count firmware modules to both installer Lua directories"
    shasum -a 256 \
        "$INSTALLER_LUA_DIR"/shellpp_ii-*.bin \
        "$INSTALLER_PACKAGED_DIR"/shellpp_ii-*.bin \
        "$INSTALLER_DIR/resource.bin" "$INSTALLER_DIR/hashCode"
fi
