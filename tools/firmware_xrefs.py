#!/usr/bin/env python3
"""Find Thumb literal-pool references in the exact XiaomiVela AP image."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


def parse_address(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("addresses", nargs="+", type=parse_address)
    parser.add_argument("--base", type=parse_address, default=0x2C000000)
    args = parser.parse_args()

    try:
        sys.path.insert(0, "/tmp/shellpp-capstone")
        from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs
        from capstone.arm import ARM_OP_MEM, ARM_REG_PC
    except ImportError as error:
        raise SystemExit(f"capstone is required: {error}")

    image = args.image.read_bytes()
    targets = set(args.addresses)
    disassembler = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    disassembler.detail = True

    hits: dict[int, list[tuple[int, str]]] = {target: [] for target in targets}
    for instruction in disassembler.disasm(image, args.base):
        for operand in instruction.operands:
            if operand.type != ARM_OP_MEM or operand.mem.base != ARM_REG_PC:
                continue
            literal = ((instruction.address + 4) & ~3) + operand.mem.disp
            offset = literal - args.base
            if offset < 0 or offset + 4 > len(image):
                continue
            value = struct.unpack_from("<I", image, offset)[0]
            if value in targets:
                hits[value].append((instruction.address, f"{instruction.mnemonic} {instruction.op_str}"))

    for target in args.addresses:
        print(f"target {target:#010x}")
        for address, text in hits[target]:
            print(f"  {address:#010x}: {text}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
