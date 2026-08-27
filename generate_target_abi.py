#!/usr/bin/env python3
"""Generate the nativeApp ABI header from one Shell++ firmware profile."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


PROFILE_KEY = re.compile(r"^[A-Z][A-Z0-9_]*$")
VERSION = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
SHELL_META = frozenset(chr(code) for code in (34, 39, 92, 96, 36, 59, 38, 124, 60, 62, 40, 41, 123, 125, 91, 93, 42, 63, 33))

METADATA_KEYS = (
    "TARGET_ID",
    "FIRMWARE_VERSION",
    "FIRMWARE_CODE",
    "FIRMWARE_IMAGE",
    "FIRMWARE_IMAGE_SIZE",
    "FIRMWARE_IMAGE_SHA256",
    "CPU",
    "FLOAT_ABI",
    "MAX_LOADED_SIZE",
    "MAX_BSS_SIZE",
)

ABI_KEYS = (
    "ABI_REGISTER_DRIVER_ADDR",
    "ABI_UNREGISTER_DRIVER_ADDR",
    "ABI_OPEN_ADDR",
    "ABI_READ_ADDR",
    "ABI_WRITE_ADDR",
    "ABI_CLOSE_ADDR",
    "ABI_LSEEK_ADDR",
    "ABI_UNLINK_ADDR",
    "ABI_RENAME_ADDR",
    "ABI_OPENDIR_ADDR",
    "ABI_CLOSEDIR_ADDR",
    "ABI_READDIR_ADDR",
    "ABI_RMDIR_ADDR",
    "ABI_APP_LOOKUP_ADDR",
    "ABI_APP_INSTALL_ADDR",
    "ABI_LAUNCHER_ADD_ADDR",
    "ABI_NOTIFICATION_SUBMIT_ADDR",
    "ABI_LVX_CONTENT_CREATE_ADDR",
    "ABI_LVX_PAGE_TITLE_CREATE_ADDR",
    "ABI_LVX_LABEL_CREATE_ADDR",
    "ABI_LVX_LABEL_SET_TEXT_ADDR",
    "ABI_LV_DISPLAY_GET_LAYER_TOP_ADDR",
    "ABI_LV_TIMER_CREATE_ADDR",
    "ABI_LV_TIMER_DELETE_ADDR",
    "ABI_LVX_OBJECT_SET_SIZE_ADDR",
    "ABI_LVX_OBJECT_ALIGN_ADDR",
    "ABI_LVX_ALIGN_TO_ADDR",
    "ABI_LVX_SET_HIDDEN_ADDR",
    "ABI_LVX_STYLE_APPLY_ADDR",
    "ABI_LVX_LIST_ROW_CREATE_ADDR",
    "ABI_LVX_LIST_ROW_UPDATE_ADDR",
    "ABI_LVX_LIST_ROW_TRAILING_ADDR",
    "ABI_LVX_EVENT_ADD_ADDR",
    "ABI_LVX_EVENT_GET_USER_DATA_ADDR",
    "ABI_LVX_EVENT_GET_CODE_ADDR",
    "ABI_ACTIVITY_NAVIGATE_ADDR",
    "ABI_ACTIVITY_FINISH_ADDR",
    "ABI_POSIX_SPAWN_ADDR",
    "ABI_FILE_ACTIONS_INIT_ADDR",
    "ABI_FILE_ACTIONS_ADDOPEN_ADDR",
    "ABI_FILE_ACTIONS_DESTROY_ADDR",
    "ABI_SPAWNATTR_INIT_ADDR",
    "ABI_SPAWNATTR_DESTROY_ADDR",
    "ABI_WAITPID_ADDR",
    "ABI_SOFT_RESTART_ADDR",
    "ABI_STYLE_MISANS_DEMIBOLD_32_ADDR",
    "ABI_O_RDONLY",
    "ABI_O_WRONLY",
    "ABI_O_CREAT",
    "ABI_O_TRUNC",
    "ABI_SEEK_SET",
    "ABI_SEEK_END",
    "ABI_DT_DIR",
    "ABI_DT_REG",
    "ABI_DT_LNK",
    "ABI_APP_DESCRIPTOR_SIZE",
    "ABI_PAGE_DESCRIPTOR_SIZE",
    "ABI_ALIGN_TOP_MID",
    "ABI_ALIGN_TOP_LEFT",
    "ABI_ALIGN_OUT_BOTTOM_MID",
    "ABI_EVENT_CLICKED",
    "ABI_TRAILING_NONE",
)


class ProfileError(ValueError):
    pass


def read_profile(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise ProfileError(f"{path}:{line_number}: expected KEY=VALUE")
        key, value = line.split("=", 1)
        if (
            not PROFILE_KEY.fullmatch(key)
            or not value
            or any(ch.isspace() or ch in SHELL_META for ch in value)
        ):
            raise ProfileError(f"{path}:{line_number}: invalid profile assignment")
        if key in values:
            raise ProfileError(f"{path}:{line_number}: duplicate key {key}")
        values[key] = value
    missing = [key for key in METADATA_KEYS + ABI_KEYS if key not in values]
    if missing:
        raise ProfileError(f"{path}: missing keys: {', '.join(missing)}")
    return values


def unsigned(value: str, key: str) -> int:
    try:
        number = int(value, 0)
    except ValueError as error:
        raise ProfileError(f"{key} is not an integer: {value}") from error
    if not 0 <= number <= 0xFFFFFFFF:
        raise ProfileError(f"{key} is outside uint32 range")
    return number


def validate(values: dict[str, str]) -> dict[str, int]:
    allowed = set(METADATA_KEYS + ABI_KEYS)
    unknown = sorted(set(values) - allowed)
    if unknown:
        raise ProfileError("unknown keys: " + ", ".join(unknown))
    if not re.fullmatch(r"[a-z0-9][a-z0-9.-]*", values["TARGET_ID"]):
        raise ProfileError(
            "TARGET_ID may contain only lowercase letters, digits, dots, and hyphens"
        )
    match = VERSION.fullmatch(values["FIRMWARE_VERSION"])
    if not match:
        raise ProfileError("FIRMWARE_VERSION must be major.minor.patch")
    major, minor, patch = (int(part) for part in match.groups())
    if minor > 999 or patch > 999:
        raise ProfileError("firmware minor and patch components must be below 1000")
    expected_code = major * 1_000_000 + minor * 1_000 + patch
    if unsigned(values["FIRMWARE_CODE"], "FIRMWARE_CODE") != expected_code:
        raise ProfileError(f"FIRMWARE_CODE must be {expected_code}")
    if not re.fullmatch(r"[0-9a-f]{64}", values["FIRMWARE_IMAGE_SHA256"]):
        raise ProfileError("FIRMWARE_IMAGE_SHA256 must be lowercase SHA-256")
    unsigned(values["FIRMWARE_IMAGE_SIZE"], "FIRMWARE_IMAGE_SIZE")
    unsigned(values["MAX_LOADED_SIZE"], "MAX_LOADED_SIZE")
    unsigned(values["MAX_BSS_SIZE"], "MAX_BSS_SIZE")

    numbers = {key: unsigned(values[key], key) for key in ABI_KEYS}
    function_keys = [key for key in ABI_KEYS if key.endswith("_ADDR") and "STYLE_" not in key]
    for key in function_keys:
        if numbers[key] == 0 or numbers[key] & 1 == 0:
            raise ProfileError(f"{key} must be a nonzero Thumb address")
    if numbers["ABI_STYLE_MISANS_DEMIBOLD_32_ADDR"] & 3:
        raise ProfileError("ABI_STYLE_MISANS_DEMIBOLD_32_ADDR must be word-aligned")
    for key in ("ABI_APP_DESCRIPTOR_SIZE", "ABI_PAGE_DESCRIPTOR_SIZE"):
        if numbers[key] == 0 or numbers[key] & 3:
            raise ProfileError(f"{key} must be a nonzero multiple of four")
    return numbers


def render(values: dict[str, str], numbers: dict[str, int], profile: Path) -> str:
    lines = [
        "#ifndef SHELLPP_TARGET_ABI_H",
        "#define SHELLPP_TARGET_ABI_H",
        "",
        f"/* Generated from {profile.name}; do not edit this output. */",
        "#define SHELLPP_TARGET_ABI_GENERATED 1",
        f'#define SHELLPP_TARGET_ID "{values["TARGET_ID"]}"',
        f'#define SHELLPP_TARGET_FIRMWARE_VERSION "{values["FIRMWARE_VERSION"]}"',
        f"#define SHELLPP_ABI_FIRMWARE_CODE {int(values['FIRMWARE_CODE'], 0)}u",
    ]
    for key in ABI_KEYS:
        macro = "SHELLPP_" + key
        lines.append(f"#define {macro} 0x{numbers[key]:08x}u")
    lines.extend(("", "#endif", ""))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        values = read_profile(args.profile)
        numbers = validate(values)
        content = render(values, numbers, args.profile)
    except (OSError, ProfileError) as error:
        parser.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
