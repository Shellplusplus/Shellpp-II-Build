#!/usr/bin/env python3
"""Rebuild the narrow Shell++ II watchface resource.bin safely."""

from __future__ import annotations

import argparse
import hashlib
import os
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
import struct
import sys
import tempfile

MAGIC = 0x1234A55A
BASE_HEADER_SIZE = 168
RECORD_SIZE = 16
FILE_TYPE = 5


class FormatError(ValueError):
    pass


@dataclass(frozen=True)
class Record:
    table_offset: int
    uid: int
    kind: int
    address: int
    length: int


@dataclass(frozen=True)
class Replacement:
    record: Record
    name: str
    payload: bytes


def u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def require_range(data: bytes | bytearray, offset: int, length: int, label: str) -> None:
    if offset < 0 or length < 0 or offset + length > len(data):
        raise FormatError(f"{label} is outside resource.bin")


def parse_records(data: bytes) -> list[Record]:
    require_range(data, 0, BASE_HEADER_SIZE, "header")
    if u32(data, 0) != MAGIC:
        raise FormatError(f"unexpected resource magic: 0x{u32(data, 0):08x}")
    color_count, theme_count, recolor_count = data[24], data[28], data[29]
    header_size = BASE_HEADER_SIZE + (recolor_count or color_count) * 4
    protocol_minor = (u32(data, 16) >> 8) & 0xFF
    type_count = 12 if protocol_minor > 8 else 10
    table_size = 8 + type_count * 8
    require_range(data, 0, header_size, "color table")

    position = header_size
    locations: list[tuple[int, int, int]] = []
    for theme_index in range(theme_count):
        require_range(data, position, table_size, f"theme {theme_index} table")
        for kind in range(type_count):
            count = u32(data, position + 8 + kind * 8)
            offset = u32(data, position + 12 + kind * 8)
            if count:
                locations.append((offset, count, kind))
        position += table_size
        require_range(data, position, 72, f"theme {theme_index} extension")
        group_count = u32(data, position + 68) >> 2
        if group_count > 64:
            raise FormatError(f"theme {theme_index} has invalid color-group count")
        position += 72 + group_count * 4

    records: list[Record] = []
    seen_offsets: set[int] = set()
    for offset, count, expected_kind in locations:
        require_range(data, offset, count * RECORD_SIZE, "record table")
        if offset < position:
            raise FormatError("record table overlaps theme area")
        for index in range(count):
            table_offset = offset + index * RECORD_SIZE
            if table_offset in seen_offsets:
                raise FormatError("duplicate record table entry")
            seen_offsets.add(table_offset)
            uid, _flags, address, length = struct.unpack_from("<IIII", data, table_offset)
            kind = uid >> 24
            if kind != expected_kind:
                raise FormatError(f"record 0x{uid:08x} type {kind}, expected {expected_kind}")
            require_range(data, address, length, f"record 0x{uid:08x} payload")
            records.append(Record(table_offset, uid, kind, address, length))
    if not records:
        raise FormatError("resource.bin contains no records")
    return records


def read_uidmap(project: Path) -> dict[str, int]:
    path = project / "uidmap.map"
    if not path.is_file():
        raise FormatError(f"uidmap is missing: {path}")
    result: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        if ":" not in line:
            raise FormatError(f"invalid uidmap line: {line!r}")
        name, raw_uid = line.split(":", 1)
        try:
            result[name.strip()] = int(raw_uid.strip(), 16)
        except ValueError as exc:
            raise FormatError(f"invalid uidmap value: {line!r}") from exc
    return result


def replacement_for(data: bytes, record: Record, root: Path, uidmap: dict[str, int]) -> Replacement:
    if record.length < 20:
        raise FormatError(f"file record 0x{record.uid:08x} is too short")
    payload = data[record.address:record.address + record.length]
    packed = u32(payload, 0)
    name_length, body_length = packed >> 24, packed & 0xFFFFFF
    if 20 + name_length + body_length != record.length:
        raise FormatError(f"file record 0x{record.uid:08x} has invalid length")
    name_bytes = payload[20:20 + name_length]
    try:
        name = name_bytes.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise FormatError(f"file record 0x{record.uid:08x} has invalid name") from exc
    relative = PurePosixPath(name)
    if relative.is_absolute() or ".." in relative.parts:
        raise FormatError(f"unsafe embedded resource name: {name!r}")
    source = (root.joinpath(*relative.parts)).resolve()
    try:
        source.relative_to(root)
    except ValueError as exc:
        raise FormatError(f"embedded resource escapes resources/: {name!r}") from exc
    if not source.is_file():
        raise FormatError(f"missing source for embedded resource: {name}")
    expected_uid = record.uid & 0x0FFFFFFF
    if uidmap.get(name) != expected_uid:
        raise FormatError(f"uidmap mismatch for {name}")
    body = source.read_bytes()
    if len(body) > 0xFFFFFF:
        raise FormatError(f"embedded resource is too large: {name}")
    rebuilt = struct.pack("<I", (name_length << 24) | len(body)) + payload[4:20] + name_bytes + body
    return Replacement(record, name, rebuilt)


def atomic_write(path: Path, content: bytes) -> None:
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(mode="wb", dir=path.parent, prefix=f".{path.name}.", delete=False) as stream:
            temporary = stream.name
            stream.write(content)
        os.replace(temporary, path)
    finally:
        if temporary and os.path.exists(temporary):
            os.unlink(temporary)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def update_hash_code(project: Path) -> None:
    paths = [project / "capability.json", project / "resources" / "manifest.xml", project / "resource.bin"]
    if any(not path.is_file() for path in paths):
        raise FormatError("cannot update hashCode: project metadata is incomplete")
    atomic_write(project / "hashCode", ",".join(sha256(path) for path in paths).encode("ascii"))


def rebuild(project: Path) -> tuple[int, int, list[str]]:
    resource_path = project / "resource.bin"
    root = (project / "resources").resolve()
    if not resource_path.is_file() or not root.is_dir():
        raise FormatError("project must contain resource.bin and resources/")
    original = resource_path.read_bytes()
    records = parse_records(original)
    uidmap = read_uidmap(project)
    replacements = [replacement_for(original, record, root, uidmap) for record in records if record.kind == FILE_TYPE]
    if not replacements:
        raise FormatError("resource.bin contains no File records")
    replacements.sort(key=lambda item: item.record.address)
    first_address = replacements[0].record.address
    file_tables = {item.record.table_offset for item in replacements}
    cursor = first_address
    for record in sorted(records, key=lambda item: item.address):
        if record.address < first_address:
            continue
        if record.table_offset not in file_tables or record.address != cursor:
            raise FormatError("File payloads are not contiguous at resource.bin end")
        cursor += record.length
    if cursor != len(original):
        raise FormatError("resource.bin has trailing data after File payloads")

    rebuilt = bytearray(original[:first_address])
    cursor = first_address
    for item in replacements:
        if item.record.table_offset + RECORD_SIZE > first_address:
            raise FormatError("File record table is not before payload")
        struct.pack_into("<I", rebuilt, item.record.table_offset + 8, cursor)
        struct.pack_into("<I", rebuilt, item.record.table_offset + 12, len(item.payload))
        rebuilt.extend(item.payload)
        cursor += len(item.payload)
    for item in replacements:
        _uid, _flags, address, length = struct.unpack_from("<IIII", rebuilt, item.record.table_offset)
        if bytes(rebuilt[address:address + length]) != item.payload:
            raise FormatError(f"failed to verify rebuilt resource: {item.name}")
    atomic_write(resource_path, bytes(rebuilt))
    update_hash_code(project)
    return len(original), len(rebuilt), [item.name for item in replacements]


def main() -> int:
    parser = argparse.ArgumentParser(description="Rebuild Shell++ II resource.bin")
    parser.add_argument("--project", type=Path, required=True)
    args = parser.parse_args()
    try:
        old_size, new_size, names = rebuild(args.project.resolve())
    except (FormatError, OSError, struct.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(f"repacked {len(names)} resources ({old_size} -> {new_size} bytes): {', '.join(names)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
