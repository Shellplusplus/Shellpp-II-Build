#!/bin/sh
# Build the standalone NuttX loader-first module for one exact firmware ABI.
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../shellpp-ii" && pwd)
SOURCE_DIR="$PROJECT_DIR/module"
TARGET_ENV="$PROJECT_DIR/targets/xiaomi-band-10-pro-3.101.036.env"
INSTALLER_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../shellpp-ii-installer" && pwd)
INSTALLER_SOURCE_DIR="$INSTALLER_DIR/_Lua"
INSTALLER_EDITOR_DIR="$INSTALLER_DIR/_Lua"
# manifest.xml packages this directory, rather than the editor-facing _Lua
# directory above. Keep the two trees byte-identical on every build.
INSTALLER_PACKAGED_DIR="$INSTALLER_DIR/resources/_lua/_Lua"
INSTALLER_MANIFEST="$INSTALLER_DIR/resources/manifest.xml"
FROZEN_LUA_SHA256=36c93680345db23c1cd3d6c934d9984de2d91891b2259a811aedc17ef5559f48
FROZEN_SUPERVISOR_SHA256=fbd5860c92392621b261955c3d6f30321194a0c956e4bfdfba99a73f844e3d79

. "$TARGET_ENV"

CLANG_BIN="$CLANG"
if [ -z "$CLANG_BIN" ]; then CLANG_BIN=/usr/bin/clang; fi

LLD_BIN="$RUST_LLD"
if [ -z "$LLD_BIN" ]; then
    LLD_BIN=/Users/ikun_cxkpro/.rustup/toolchains/stable-x86_64-apple-darwin/lib/rustlib/x86_64-apple-darwin/bin/rust-lld
fi

PYTHON_BIN="$PYTHON"
if [ -z "$PYTHON_BIN" ]; then PYTHON_BIN=/usr/local/bin/python3; fi

NODE_BIN=${NODE:-}
if [ -z "$NODE_BIN" ]; then NODE_BIN=$(command -v node || true); fi

if [ ! -x "$CLANG_BIN" ]; then
    echo "clang not executable: $CLANG_BIN" >&2
    exit 1
fi
if [ ! -x "$LLD_BIN" ]; then
    echo "rust-lld not executable: $LLD_BIN" >&2
    echo "Set RUST_LLD to an ARM-capable lld binary." >&2
    exit 1
fi
if [ ! -x "$PYTHON_BIN" ]; then
    echo "python not executable: $PYTHON_BIN" >&2
    exit 1
fi
if [ -z "$NODE_BIN" ] || [ ! -x "$NODE_BIN" ]; then
    echo "node is required to generate the launcher icon" >&2
    exit 1
fi

verify_installer_payload() {
    payload_dir=$1
    payload_label=$2

    [ -d "$payload_dir" ] || {
        echo "installer $payload_label directory is missing: $payload_dir" >&2
        exit 1
    }

    for payload_name in main.lua shellpp_ii.bin shellpp_ii_icon.bin; do
        if [ ! -f "$payload_dir/$payload_name" ] || [ -L "$payload_dir/$payload_name" ]; then
            echo "installer $payload_label payload must be a regular file: $payload_dir/$payload_name" >&2
            exit 1
        fi
    done

    payload_count=$(find "$payload_dir" -mindepth 1 -maxdepth 1 -print | wc -l | tr -d '[:space:]')
    if [ "$payload_count" != 3 ]; then
        echo "installer $payload_label payload must contain only main.lua, shellpp_ii.bin, and shellpp_ii_icon.bin" >&2
        find "$payload_dir" -mindepth 1 -maxdepth 1 -print >&2
        exit 1
    fi

    unexpected_payload=$(find "$payload_dir" -mindepth 1 -maxdepth 1 \
        ! -name main.lua ! -name shellpp_ii.bin ! -name shellpp_ii_icon.bin -print -quit)
    if [ -n "$unexpected_payload" ]; then
        echo "unexpected installer $payload_label payload: $unexpected_payload" >&2
        exit 1
    fi
}

verify_installer_manifest() {
    [ -f "$INSTALLER_MANIFEST" ] || {
        echo "installer manifest is missing: $INSTALLER_MANIFEST" >&2
        exit 1
    }

    manifest_file_count=$(grep -c '<File[[:space:]]' "$INSTALLER_MANIFEST" || true)
    if [ "$manifest_file_count" != 3 ]; then
        echo "installer manifest must declare exactly one Lua file and two bin files" >&2
        exit 1
    fi

    for manifest_path in \
        _lua/_Lua/main.lua \
        _lua/_Lua/shellpp_ii.bin \
        _lua/_Lua/shellpp_ii_icon.bin; do
        expected_entry="<File fileName=\"$manifest_path\" name=\"$manifest_path\"/>"
        entry_count=$(grep -F -c "$expected_entry" "$INSTALLER_MANIFEST" || true)
        if [ "$entry_count" != 1 ]; then
            echo "installer manifest must declare exactly once: $manifest_path" >&2
            exit 1
        fi
    done
}

# The installer Lua owns the already-validated loader sequence.  Refuse to
# build before touching any output when the editor and packaged copies differ;
# a native UI build must never repair or overwrite either copy.
[ -d "$INSTALLER_SOURCE_DIR" ] || {
    echo "installer source directory is missing: $INSTALLER_SOURCE_DIR" >&2
    exit 1
}
[ -d "$INSTALLER_PACKAGED_DIR" ] || {
    echo "installer packaged resource directory is missing: $INSTALLER_PACKAGED_DIR" >&2
    exit 1
}
verify_installer_payload "$INSTALLER_SOURCE_DIR" "source"
verify_installer_payload "$INSTALLER_PACKAGED_DIR" "packaged"
verify_installer_manifest
if [ ! -f "$INSTALLER_SOURCE_DIR/main.lua" ]; then
    echo "installer source main.lua is missing: $INSTALLER_SOURCE_DIR/main.lua" >&2
    exit 1
fi
if [ ! -f "$INSTALLER_PACKAGED_DIR/main.lua" ]; then
    echo "installer packaged main.lua is missing: $INSTALLER_PACKAGED_DIR/main.lua" >&2
    exit 1
fi
if ! cmp -s "$INSTALLER_SOURCE_DIR/main.lua" "$INSTALLER_PACKAGED_DIR/main.lua"; then
    echo "refusing to overwrite frozen Lua: installer copies differ" >&2
    exit 1
fi

sha256_of() {
    shasum -a 256 "$1" | awk '{print $1}'
}

if [ "$(sha256_of "$INSTALLER_SOURCE_DIR/main.lua")" != "$FROZEN_LUA_SHA256" ]; then
    echo "refusing to build: frozen installer Lua was modified" >&2
    exit 1
fi
if [ "$(sha256_of "$SOURCE_DIR/src/supervisor.c")" != "$FROZEN_SUPERVISOR_SHA256" ]; then
    echo "refusing to build: frozen Supervisor was modified" >&2
    exit 1
fi

OUT_DIR="$SCRIPT_DIR/out/$TARGET_ID"
MODULE_PATH="$OUT_DIR/shellpp_ii.bin"
MODULE_OBJECT="$OUT_DIR/supervisor.o"
NATIVE_APP_OBJECT="$OUT_DIR/native_app.o"
NATIVE_FS_OBJECT="$OUT_DIR/native_fs.o"
NATIVE_UI_OBJECT="$OUT_DIR/native_ui.o"
PRELUDE_OBJECT="$OUT_DIR/module_prelude.o"

mkdir -p "$OUT_DIR"

compile_source() {
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
        -I "$SOURCE_DIR/include" \
        -c "$1" \
        -o "$2"
}

compile_source "$SOURCE_DIR/src/supervisor.c" "$MODULE_OBJECT"
compile_source "$SOURCE_DIR/src/native_app.c" "$NATIVE_APP_OBJECT"
compile_source "$SOURCE_DIR/src/native_fs.c" "$NATIVE_FS_OBJECT"
compile_source "$SOURCE_DIR/src/native_ui.c" "$NATIVE_UI_OBJECT"
compile_source "$SOURCE_DIR/src/module_prelude.S" "$PRELUDE_OBJECT"

"$LLD_BIN" \
    -flavor gnu \
    -m armelf \
    -r \
    -T "$SCRIPT_DIR/shellpp_ii.ld" \
    -u module_initialize \
    -o "$MODULE_PATH" \
    "$PRELUDE_OBJECT" \
    "$MODULE_OBJECT" \
    "$NATIVE_APP_OBJECT" \
    "$NATIVE_FS_OBJECT" \
    "$NATIVE_UI_OBJECT"

"$PYTHON_BIN" "$SCRIPT_DIR/verify_elf.py" \
    --max-loaded-size "$MAX_LOADED_SIZE" \
    --max-bss-size 24576 \
    "$MODULE_PATH"

ICON_SOURCE="$PROJECT_DIR/assets/shellpp-ii-icon.png"
ICON_BMP="$OUT_DIR/shellpp-ii-icon.bmp"
if [ ! -f "$ICON_SOURCE" ]; then
    echo "Shell++ icon source is missing: $ICON_SOURCE" >&2
    exit 1
fi
if ! command -v sips >/dev/null 2>&1; then
    echo "sips is required to generate the launcher icon on macOS" >&2
    exit 1
fi
sips -s format bmp "$ICON_SOURCE" --out "$ICON_BMP" >/dev/null
"$NODE_BIN" "$SCRIPT_DIR/make_icon_bin.js" "$ICON_BMP" \
    "$INSTALLER_SOURCE_DIR/shellpp_ii_icon.bin"
if [ ! -f "$INSTALLER_SOURCE_DIR/shellpp_ii_icon.bin" ]; then
    echo "installer icon is missing: $INSTALLER_SOURCE_DIR/shellpp_ii_icon.bin" >&2
    exit 1
fi

# Lua remains frozen. Synchronize only generated native artifacts into the
# tree selected by manifest.xml.
cp "$INSTALLER_SOURCE_DIR/shellpp_ii_icon.bin" "$INSTALLER_PACKAGED_DIR/shellpp_ii_icon.bin"
cp "$MODULE_PATH" "$INSTALLER_EDITOR_DIR/shellpp_ii.bin"
cp "$MODULE_PATH" "$INSTALLER_PACKAGED_DIR/shellpp_ii.bin"

cmp -s "$INSTALLER_SOURCE_DIR/main.lua" "$INSTALLER_PACKAGED_DIR/main.lua"
cmp -s "$INSTALLER_SOURCE_DIR/shellpp_ii_icon.bin" "$INSTALLER_PACKAGED_DIR/shellpp_ii_icon.bin"
cmp -s "$MODULE_PATH" "$INSTALLER_EDITOR_DIR/shellpp_ii.bin"
cmp -s "$MODULE_PATH" "$INSTALLER_PACKAGED_DIR/shellpp_ii.bin"
verify_installer_payload "$INSTALLER_SOURCE_DIR" "source"
verify_installer_payload "$INSTALLER_PACKAGED_DIR" "packaged"
verify_installer_manifest

echo "Built $MODULE_PATH"
echo "Synchronized editor resources: $INSTALLER_EDITOR_DIR"
echo "Synchronized manifest resources: $INSTALLER_PACKAGED_DIR"
shasum -a 256 "$INSTALLER_PACKAGED_DIR/main.lua" \
    "$INSTALLER_PACKAGED_DIR/shellpp_ii.bin" \
    "$INSTALLER_PACKAGED_DIR/shellpp_ii_icon.bin"
echo "The manifest resource directory is ready for your private watchface packer."
