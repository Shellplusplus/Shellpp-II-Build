#!/usr/bin/env python3
"""Reject Native ELF deployment until N67CN 3.1.175 loader evidence exists."""

from __future__ import annotations

import argparse
import hashlib
import os
import sys
from pathlib import Path


ELF_MAGIC = b"\x7fELF"


def iter_files(root: Path) -> list[Path]:
    if not root.is_dir():
        raise ValueError(f"not a directory: {root}")
    result: list[Path] = []
    for current, directories, names in os.walk(root):
        directories[:] = sorted(name for name in directories if name not in {".git", "__pycache__"})
        result.extend(Path(current) / name for name in sorted(names))
    return result


def verify_absent(root: Path) -> int:
    matches: list[str] = []
    for path in iter_files(root):
        try:
            if ELF_MAGIC in path.read_bytes():
                matches.append(str(path))
        except OSError as error:
            print(f"FAIL: cannot read {path}: {error}", file=sys.stderr)
            return 1
    if matches:
        print("FAIL: Native-module ELF content is forbidden for N67CN 3.1.175:", file=sys.stderr)
        for match in matches:
            print(f"  {match}", file=sys.stderr)
        return 1
    print(f"OK: no ELF content under {root}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", nargs="?", type=Path)
    parser.add_argument("--assert-no-elf", type=Path, metavar="DIRECTORY")
    args = parser.parse_args()
    if bool(args.module) == bool(args.assert_no_elf):
        parser.error("provide exactly one of a candidate module or --assert-no-elf DIRECTORY")
    if args.assert_no_elf:
        try:
            return verify_absent(args.assert_no_elf.resolve())
        except ValueError as error:
            print(f"FAIL: {error}", file=sys.stderr)
            return 1
    try:
        digest = hashlib.sha256(args.module.read_bytes()).hexdigest()
    except OSError as error:
        print(f"FAIL: cannot read candidate: {error}", file=sys.stderr)
        return 1
    print("BLOCKED: N67CN v3.1.175 has no verified external Native-module ABI.", file=sys.stderr)
    print(f"Candidate SHA-256: {digest}", file=sys.stderr)
    print("Missing evidence: loader, ET_REL acceptance, entry convention, relocations, imports, driver and callback ABIs.", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
