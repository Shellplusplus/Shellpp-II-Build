#!/usr/bin/env python3
"""Verify a Shell++ ARM ET_REL module against its generated target ABI."""

from __future__ import annotations

import argparse
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
SECTION_HEADER = struct.Struct("<IIIIIIIIII")
SYMBOL = struct.Struct("<IIIBBH")
RELOCATION = struct.Struct("<II")

ET_REL = 1
EM_ARM = 40
EV_CURRENT = 1
ELFCLASS32 = 1
ELFDATA2LSB = 1
EV_SYSV = 0
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHT_RELA = 4
SHT_NOBITS = 8
SHT_REL = 9
SHF_ALLOC = 0x2
SHF_WRITE = 0x1
SHF_EXECINSTR = 0x4
SHN_UNDEF = 0
STT_FUNC = 2
R_ARM_ABS32 = 2
R_ARM_TARGET1 = 38

ABI_DEFINE = re.compile(
    r"^#define\s+(SHELLPP_ABI_[A-Z0-9_]+_ADDR)\s+"
    r"(0x[0-9a-fA-F]+|[0-9]+)u?$"
)
DATA_ADDRESS_MACROS = {"SHELLPP_ABI_STYLE_MISANS_DEMIBOLD_32_ADDR"}


class VerificationError(Exception):
    pass


@dataclass(frozen=True)
class Section:
    name_offset: int
    type: int
    flags: int
    address: int
    offset: int
    size: int
    link: int
    info: int
    alignment: int
    entry_size: int
    name: str = ""


@dataclass(frozen=True)
class ElfSymbol:
    name: str
    value: int
    size: int
    info: int
    other: int
    section_index: int


def fail(message: str) -> None:
    raise VerificationError(message)


def read_abi_addresses(path: Path) -> tuple[set[int], set[int]]:
    functions: set[int] = set()
    data_addresses: set[int] = set()
    for raw in path.read_text(encoding="ascii").splitlines():
        match = ABI_DEFINE.fullmatch(raw.strip())
        if not match:
            continue
        name, value_text = match.groups()
        value = int(value_text, 0)
        if name in DATA_ADDRESS_MACROS:
            data_addresses.add(value)
        else:
            functions.add(value)
    if not functions:
        fail("generated ABI header contains no firmware function addresses")
    if not data_addresses:
        fail("generated ABI header contains no firmware data addresses")
    if any((address & 1) == 0 for address in functions):
        fail("generated ABI header contains a non-Thumb function address")
    return functions, data_addresses


def require_range(data: bytes, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(data):
        fail(label + " is outside the ELF file")


def c_string(data: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        fail("string offset is outside its string table")
    end = data.find(b"\0", offset)
    if end < 0:
        fail("unterminated string in ELF string table")
    return data[offset:end].decode("ascii", "replace")


def has_flag(value: int, flag: int) -> bool:
    return (value & flag) == flag


def section_footprint(sections: list[Section]) -> int:
    footprint = 0
    for section in sections:
        if not has_flag(section.flags, SHF_ALLOC):
            continue
        alignment = max(section.alignment, 1)
        footprint = (footprint + alignment - 1) // alignment * alignment
        footprint += section.size
    return footprint


def literal_ranges(
    sections: list[Section], symbols: list[ElfSymbol]
) -> dict[int, list[tuple[int, int]]]:
    result: dict[int, list[tuple[int, int]]] = {}
    for section_index, section in enumerate(sections):
        if section.name != ".text":
            continue
        mappings = sorted(
            (symbol.value, symbol.name)
            for symbol in symbols
            if symbol.section_index == section_index
            and (
                symbol.name in ("$d", "$t")
                or symbol.name.startswith("$d.")
                or symbol.name.startswith("$t.")
            )
        )
        ranges: list[tuple[int, int]] = []
        for index, (start, kind) in enumerate(mappings):
            end = mappings[index + 1][0] if index + 1 < len(mappings) else section.size
            if (kind == "$d" or kind.startswith("$d.")) and start < end:
                ranges.append((start, end))
        result[section_index] = ranges
    return result


def scan_absolute_addresses(
    data: bytes,
    sections: list[Section],
    symbols: list[ElfSymbol],
    relocation_offsets: set[tuple[int, int]],
    firmware_functions: set[int],
    firmware_data: set[int],
) -> set[int]:
    ranges_by_section = literal_ranges(sections, symbols)
    found: set[int] = set()
    for section_index, section in enumerate(sections):
        if not has_flag(section.flags, SHF_ALLOC) or section.type == SHT_NOBITS:
            continue
        if section.name == ".text":
            ranges = ranges_by_section.get(section_index, [])
        elif section.name == ".data":
            ranges = [(0, section.size)]
        else:
            ranges = []
        for range_start, range_end in ranges:
            aligned_start = (range_start + 3) & ~3
            for relative in range(aligned_start, range_end - 3, 4):
                if (section_index, relative) in relocation_offsets:
                    continue
                value = struct.unpack_from("<I", data, section.offset + relative)[0]
                location = section.name + "+0x" + format(relative, "x")
                if value in firmware_functions or value in firmware_data:
                    found.add(value)
                if 0x2C000000 <= value < 0x2D000000:
                    fail(
                        "analysis-only firmware address at "
                        + location
                        + ": 0x"
                        + format(value, "08x")
                    )
                if 0x0C000000 <= value < 0x0D000000:
                    if (value & 1) == 0:
                        fail(
                            "firmware function address is not Thumb at "
                            + location
                            + ": 0x"
                            + format(value, "08x")
                        )
                    if value not in firmware_functions:
                        fail(
                            "firmware function is not in the target whitelist at "
                            + location
                            + ": 0x"
                            + format(value, "08x")
                        )
                if (
                    0x20000000 <= value < 0x21000000
                    and not 0x20200000 <= value < 0x20300000
                    and value not in firmware_data
                ):
                    fail(
                        "firmware data address is not in the target whitelist at "
                        + location
                        + ": 0x"
                        + format(value, "08x")
                    )
    # Clang may fold nearby absolute call targets into one literal base plus
    # an immediate offset. Therefore an address can be used without appearing
    # byte-for-byte in the literal pool. The scan remains fail-closed for every
    # absolute firmware address that is emitted directly.
    return found


def parse_elf(
    path: Path, firmware_functions: set[int], firmware_data: set[int]
) -> tuple[dict[str, int], list[Section], list[ElfSymbol], set[int], set[int]]:
    data = path.read_bytes()
    if len(data) < ELF_HEADER.size:
        fail("file is smaller than an ELF32 header")
    header = ELF_HEADER.unpack_from(data)
    ident = header[0]
    e_type, e_machine, e_version, e_entry = header[1:5]
    e_phoff, e_shoff, e_flags = header[5:8]
    e_ehsize, _e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx = header[8:]
    if ident[:4] != b"\x7fELF":
        fail("missing ELF magic")
    if ident[4] != ELFCLASS32 or ident[5] != ELFDATA2LSB or ident[6] != EV_CURRENT:
        fail("expected ELF32 little-endian version 1")
    if ident[7] != EV_SYSV:
        fail("expected System V ELF ABI")
    if e_type != ET_REL or e_machine != EM_ARM or e_version != EV_CURRENT:
        fail("expected ARM ELF32 ET_REL")
    if ((e_flags >> 24) & 0xFF) != 5:
        fail("expected ARM EABI5 e_flags")
    if (e_flags & 0x00FFFFFF) not in (0, 0x200):
        fail("unsupported ARM float ABI flags")
    if e_ehsize != ELF_HEADER.size or e_phnum != 0 or e_phoff != 0:
        fail("ET_REL must not contain program headers")
    if e_shentsize != SECTION_HEADER.size or e_shnum == 0 or e_shstrndx >= e_shnum:
        fail("invalid section-header table")
    require_range(data, e_shoff, e_shnum * e_shentsize, "section-header table")

    sections: list[Section] = []
    for index in range(e_shnum):
        values = SECTION_HEADER.unpack_from(data, e_shoff + index * e_shentsize)
        section = Section(*values)
        if section.type != SHT_NOBITS:
            require_range(data, section.offset, section.size, "section " + str(index))
        sections.append(section)
    names_section = sections[e_shstrndx]
    if names_section.type != SHT_STRTAB:
        fail("section-name table is not a string table")
    names = data[names_section.offset : names_section.offset + names_section.size]
    sections = [
        Section(
            section.name_offset,
            section.type,
            section.flags,
            section.address,
            section.offset,
            section.size,
            section.link,
            section.info,
            section.alignment,
            section.entry_size,
            c_string(names, section.name_offset) if section.name_offset else "",
        )
        for section in sections
    ]

    symbol_sections = [section for section in sections if section.type == SHT_SYMTAB]
    if len(symbol_sections) != 1:
        fail("expected exactly one .symtab section")
    symbol_section = symbol_sections[0]
    if symbol_section.name != ".symtab" or symbol_section.entry_size != SYMBOL.size:
        fail("symbol table format is invalid")
    if symbol_section.link >= len(sections):
        fail("symbol table string-table link is invalid")
    strings_section = sections[symbol_section.link]
    if strings_section.type != SHT_STRTAB or symbol_section.size % symbol_section.entry_size:
        fail("symbol table format is invalid")
    strings = data[strings_section.offset : strings_section.offset + strings_section.size]
    symbols: list[ElfSymbol] = []
    for offset in range(
        symbol_section.offset,
        symbol_section.offset + symbol_section.size,
        symbol_section.entry_size,
    ):
        values = SYMBOL.unpack_from(data, offset)
        name = c_string(strings, values[0]) if values[0] else ""
        symbols.append(ElfSymbol(name, *values[1:]))

    undefined = [
        symbol.name or "<anonymous>"
        for symbol in symbols[1:]
        if symbol.section_index == SHN_UNDEF
    ]
    if undefined:
        fail("undefined imports are not allowed: " + ", ".join(undefined))
    entry_symbols = [symbol for symbol in symbols if symbol.name == "module_initialize"]
    if len(entry_symbols) != 1:
        fail("module_initialize must be exported exactly once")
    entry_symbol = entry_symbols[0]
    if entry_symbol.section_index == SHN_UNDEF or (entry_symbol.info & 0x0F) != STT_FUNC:
        fail("module_initialize is not a defined function")
    if e_entry != 0:
        fail("XiaomiVela ET_REL modules must have a zero ELF entry")

    required_sections = {
        ".text": (SHF_ALLOC | SHF_EXECINSTR, False),
        ".rodata": (SHF_ALLOC, False),
        ".data": (SHF_ALLOC | SHF_WRITE, False),
        ".bss": (SHF_ALLOC | SHF_WRITE, True),
    }
    by_name = {section.name: section for section in sections}
    for name, (required_flags, must_be_nobits) in required_sections.items():
        section = by_name.get(name)
        if section is None:
            fail("missing required NuttX module section " + name)
        if not has_flag(section.flags, required_flags):
            fail("section " + name + " has incompatible flags")
        if must_be_nobits != (section.type == SHT_NOBITS):
            fail("section " + name + " has an incompatible type")
    allowed_alloc_sections = set(required_sections) | {".ARM.exidx", ".init_array"}
    unexpected_alloc = sorted(
        section.name or "<anonymous>"
        for section in sections
        if has_flag(section.flags, SHF_ALLOC) and section.name not in allowed_alloc_sections
    )
    if unexpected_alloc:
        fail("unexpected allocated sections: " + ", ".join(unexpected_alloc))
    if any(section.name == ".preinit_array" for section in sections):
        fail(".preinit_array is prohibited")

    relocation_offsets: set[tuple[int, int]] = set()
    for section in sections:
        if section.type == SHT_RELA:
            fail("SHT_RELA relocations are not accepted by this target profile")
        if section.type != SHT_REL:
            continue
        if section.entry_size != RELOCATION.size or section.size % section.entry_size:
            fail("invalid REL relocation section " + section.name)
        if section.info == 0 or section.info >= len(sections):
            fail("relocation section has no valid target: " + section.name)
        for offset in range(section.offset, section.offset + section.size, section.entry_size):
            relocation_offset, _ = RELOCATION.unpack_from(data, offset)
            relocation_offsets.add((section.info, relocation_offset))

    found_addresses = scan_absolute_addresses(
        data,
        sections,
        symbols,
        relocation_offsets,
        firmware_functions,
        firmware_data,
    )
    relocations: set[int] = set()
    for section in sections:
        if section.type != SHT_REL:
            continue
        if section.link >= len(sections) or sections[section.link].type != SHT_SYMTAB:
            fail("relocation section has no valid symbol table: " + section.name)
        target = sections[section.info]
        if not has_flag(target.flags, SHF_ALLOC):
            fail("relocation target is not allocated: " + section.name)
        for offset in range(section.offset, section.offset + section.size, section.entry_size):
            relocation_offset, info = RELOCATION.unpack_from(data, offset)
            if relocation_offset + 4 > target.size:
                fail("relocation is outside target section: " + section.name)
            symbol_index = info >> 8
            if symbol_index >= len(symbols):
                fail("relocation references an out-of-range symbol")
            if symbol_index and symbols[symbol_index].section_index == SHN_UNDEF:
                fail("relocation references an undefined symbol")
            relocation_type = info & 0xFF
            allowed_types = {R_ARM_ABS32}
            if section.name == ".rel.ARM.exidx":
                allowed_types.update({0, 42})
            if section.name == ".rel.init_array":
                allowed_types.add(R_ARM_TARGET1)
            if relocation_type not in allowed_types:
                fail(
                    "unsupported relocation type "
                    + str(relocation_type)
                    + " in "
                    + section.name
                )
            relocations.add(relocation_type)
    metadata = {
        "entry": e_entry,
        "entry_symbol": entry_symbol.value,
        "loaded_size": section_footprint(sections),
        "file_size": len(data),
        "bss_size": by_name[".bss"].size,
    }
    return metadata, sections, symbols, relocations, found_addresses


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    parser.add_argument("--abi-header", type=Path, required=True)
    parser.add_argument("--max-loaded-size", type=int, required=True)
    parser.add_argument("--max-bss-size", type=int, required=True)
    args = parser.parse_args()
    try:
        firmware_functions, firmware_data = read_abi_addresses(args.abi_header)
        metadata, sections, symbols, relocations, addresses = parse_elf(
            args.module, firmware_functions, firmware_data
        )
        if metadata["loaded_size"] >= args.max_loaded_size:
            fail(
                "SHF_ALLOC size "
                + str(metadata["loaded_size"])
                + " must be strictly less than "
                + str(args.max_loaded_size)
            )
        if metadata["bss_size"] > args.max_bss_size:
            fail(
                ".bss size "
                + str(metadata["bss_size"])
                + " exceeds limit "
                + str(args.max_bss_size)
            )
    except (OSError, VerificationError) as error:
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    relocation_list = ", ".join(str(value) for value in sorted(relocations)) or "none"
    print("OK: " + str(args.module))
    print("  file size: " + str(metadata["file_size"]) + " bytes")
    print("  SHF_ALLOC total: " + str(metadata["loaded_size"]) + " bytes")
    print("  .bss: " + str(metadata["bss_size"]) + " bytes")
    print("  entry: 0x" + format(metadata["entry"], "08x"))
    print("  module_initialize: 0x" + format(metadata["entry_symbol"], "08x"))
    print("  defined symbols: " + str(len(symbols) - 1))
    print("  direct target ABI literals: " + str(len(addresses)))
    print("  ARM relocation types: " + relocation_list)
    print("  sections: " + ", ".join(section.name or "<null>" for section in sections))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
