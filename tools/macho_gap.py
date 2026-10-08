#!/usr/bin/env python3
"""Strict, read-only Mach-O static API gap report. Standard library only.

Static references are NOT measured runtime call frequencies. Unsupported or
encoded Objective-C reference pointers remain unresolved, never guessed.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

MAX_FILE = 128 * 1024 * 1024
ARM64 = 0x0100000c
TOOL_VERSION = 1


class InvalidMachO(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InvalidMachO(message)


def region(data, offset, size):
    require(0 <= offset <= len(data) and 0 <= size <= len(data) - offset,
            "Mach-O data region out of bounds")
    return data[offset:offset + size]


def cstring(data, start, end):
    require(0 <= start < end <= len(data), "Mach-O string address invalid")
    stop = data.find(b"\0", start, end)
    require(stop >= 0, "unterminated Mach-O string")
    return data[start:stop].decode("utf-8", "replace")


def arm64_slice(data):
    require(len(data) >= 32, "truncated Mach-O")
    magic = int.from_bytes(data[:4], "big")
    if magic in (0xcafebabe, 0xcafebabf):
        fat64 = magic == 0xcafebabf
        n = struct.unpack_from(">I", data, 4)[0]
        require(0 < n <= 64, "invalid fat architecture count")
        unit = 32 if fat64 else 20
        require(8 + unit * n <= len(data), "truncated fat architecture table")
        candidates = []
        for index in range(n):
            at = 8 + index * unit
            cpu = struct.unpack_from(">I", data, at)[0]
            if cpu != ARM64:
                continue
            if fat64:
                offset, size = struct.unpack_from(">QQ", data, at + 8)
            else:
                offset, size = struct.unpack_from(">II", data, at + 8)
            region(data, offset, size)
            candidates.append((offset, size))
        require(len(candidates) == 1, "expected exactly one ARM64 slice")
        start, count = candidates[0]
        return data[start:start + count]
    return data


def get_u32(data, pos):
    region(data, pos, 4)
    return struct.unpack_from("<I", data, pos)[0]


def decode_chained_imports(data, offset, length):
    blob = region(data, offset, length)
    require(len(blob) >= 28, "truncated chained fixups header")
    version, starts, imports, symbols, count, fmt, symfmt = struct.unpack_from("<7I", blob)
    require(version == 0 and symfmt == 0 and count <= 100000,
            "unsupported chained fixups header")
    # ld64.lld emits a zero-import chained-fixups header with the empty
    # imports/symbols regions positioned exactly at the payload end.
    # They are valid EMPTY ranges, not references to dereference.
    require(starts <= len(blob) and imports <= len(blob) and symbols <= len(blob)
            and (count == 0 or (imports < len(blob) and symbols < len(blob))),
            "chained fixups offset outside payload")
    require(fmt in (1, 2, 3), "unsupported chained import format")
    stride = {1: 4, 2: 8, 3: 16}[fmt]
    require(count * stride <= len(blob) - imports, "truncated chained import list")
    names = []
    for i in range(count):
        at = imports + i * stride
        if fmt == 3:
            name_offset = get_u32(blob, at + 4)
        else:
            name_offset = get_u32(blob, at) >> 9
        address = symbols + name_offset
        names.append(cstring(blob, address, len(blob)))
    return names


def read_uleb(blob, at):
    result = 0
    for i in range(10):
        require(at < len(blob), "truncated dyld ULEB128")
        b = blob[at]
        at += 1
        require(i != 9 or b < 2, "dyld ULEB128 overflow")
        result |= (b & 127) << (7 * i)
        if not b & 128:
            return result, at
    raise InvalidMachO("dyld ULEB128 too long")


def read_sleb(blob, at):
    # Values are inspected only for bounds; no arithmetic depends on them.
    for _ in range(10):
        require(at < len(blob), "truncated dyld SLEB128")
        b = blob[at]
        at += 1
        if not b & 128:
            return at
    raise InvalidMachO("dyld SLEB128 too long")


def decode_bind_stream(data, offset, length):
    blob = region(data, offset, length)
    names = []
    at = 0
    while at < len(blob):
        op = blob[at]
        at += 1
        code = op & 0xf0
        if code in (0x00, 0x10, 0x30, 0x50, 0x90):
            continue
        if code == 0x40:  # BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
            name = cstring(blob, at, len(blob))
            names.append(name)
            at += len(name.encode("utf-8")) + 1
            continue
        if code in (0x20, 0x70, 0x80, 0xa0):
            _, at = read_uleb(blob, at)
        elif code == 0xb0:
            continue
        elif code == 0xc0:
            _, at = read_uleb(blob, at)
            _, at = read_uleb(blob, at)
        elif code == 0x60:
            at = read_sleb(blob, at)
        else:
            raise InvalidMachO("unsupported dyld bind opcode " + hex(code))
    return names


def analyze(data, manifest):
    blob = arm64_slice(data)
    require(len(blob) >= 32, "truncated Mach-O header")
    magic, cpu, subtype, filetype, count, commands_size, flags, reserved = struct.unpack_from(
        "<8I", blob)
    require(magic == 0xfeedfacf and cpu == ARM64, "expected 64-bit little-endian ARM64 Mach-O")
    require(filetype in (1, 2, 6, 8), "unsupported Mach-O file type")
    require(count <= 4096 and commands_size <= len(blob) - 32,
            "invalid Mach-O load-command size")
    cursor = 32
    limit = 32 + commands_size
    sections = []
    segments = []
    imports = set()
    dylibs = []
    symtab = None
    bindings = []
    chained = []
    for _ in range(count):
        require(cursor + 8 <= limit, "truncated load command")
        command, size = struct.unpack_from("<II", blob, cursor)
        require(size >= 8 and size <= limit - cursor and size % 4 == 0,
                "invalid load command")
        if command == 0x19:
            require(size >= 72, "truncated segment command")
            fields = struct.unpack_from("<II16sQQQQiiII", blob, cursor)
            vmaddr, vmsize, fileoff, filesize = fields[3:7]
            nsects = fields[9]
            require(fileoff <= len(blob) and filesize <= len(blob) - fileoff,
                    "segment file extent invalid")
            require(nsects <= (size - 72) // 80, "truncated section array")
            segments.append((vmaddr, vmsize, fileoff, filesize))
            for j in range(nsects):
                at = cursor + 72 + 80 * j
                sec = struct.unpack_from("<16s16sQQIIIIIIII", blob, at)
                name = sec[0].split(b"\0")[0].decode("ascii", "replace")
                segment = sec[1].split(b"\0")[0].decode("ascii", "replace")
                addr, sz, off, secflags = sec[2], sec[3], sec[4], sec[8]
                if (secflags & 0xff) != 1:  # S_ZEROFILL
                    require(off <= len(blob) and sz <= len(blob) - off,
                            "section file extent invalid")
                sections.append((segment, name, addr, sz, off, secflags))
        elif command == 0x2:
            require(size >= 24, "truncated symtab command")
            symtab = struct.unpack_from("<4I", blob, cursor + 8)
        elif command == 0x80000034:
            require(size >= 16, "truncated chained fixups command")
            chained.append(struct.unpack_from("<II", blob, cursor + 8))
        elif command in (0x22, 0x80000022):
            require(size >= 48, "truncated dyld info command")
            fields = struct.unpack_from("<10I", blob, cursor + 8)
            bindings.extend((fields[2:4], fields[4:6], fields[6:8]))
        elif command in (0xc, 0x18, 0x1f, 0x80000018, 0x8000001f, 0x23):
            require(size >= 24, "truncated dependency command")
            name_at = get_u32(blob, cursor + 8)
            require(8 <= name_at < size, "dylib name offset invalid")
            dylibs.append(cstring(blob, cursor + name_at, cursor + size))
        cursor += size
    require(cursor == limit, "load-command count and size disagree")
    if symtab is not None:
        symoff, nsyms, stroff, strsize = symtab
        require(nsyms <= 1000000 and nsyms * 16 <= len(blob) - symoff,
                "invalid symbol table")
        region(blob, stroff, strsize)
        for i in range(nsyms):
            name_index, typ, sect, desc, val = struct.unpack_from(
                "<IBBHQ", blob, symoff + i * 16)
            if typ & 0xe0 or (typ & 0x0e) != 0 or not typ & 1:
                continue
            if not name_index or name_index >= strsize:
                continue
            name = cstring(blob, stroff + name_index, stroff + strsize)
            if name:
                imports.add(name)
    for off, size in chained:
        imports.update(decode_chained_imports(blob, off, size))
    for off, size in bindings:
        if size:
            imports.update(decode_bind_stream(blob, off, size))
    names = Counter()
    selector_refs = Counter()
    unresolved_refs = 0
    for segment, secname, addr, sz, off, flags in sections:
        if secname == "__objc_methname":
            raw = region(blob, off, sz)
            for item in raw.split(b"\0"):
                if item:
                    names[item.decode("utf-8", "replace")] += 1
        if secname == "__objc_selrefs":
            require(sz % 8 == 0, "selector reference section not pointer aligned")
            for at in range(off, off + sz, 8):
                pointer = struct.unpack_from("<Q", blob, at)[0]
                hit = None
                for seg, target, vm, length, target_off, typ in sections:
                    if target == "__objc_methname" and vm <= pointer < vm + length:
                        hit = cstring(blob, target_off + pointer - vm, target_off + length)
                        break
                if hit:
                    selector_refs[hit] += 1
                else:
                    unresolved_refs += 1
    classes = sorted(x.removeprefix("_OBJC_CLASS_$_") for x in imports
                     if x.startswith("_OBJC_CLASS_$_"))
    symbols = []
    for name in sorted(imports):
        detail = manifest.get("symbols", {}).get(name, {})
        symbols.append({
            "name": name, "category": "objc_class" if name.startswith("_OBJC_CLASS_$_")
            else "objc_runtime" if name.startswith("_objc_") else "symbol",
            "status": detail.get("status", "unknown"),
            "scope": detail.get("scope", ""),
            "static_occurrences": 1
        })
    return {
        "version": TOOL_VERSION,
        "sha256": hashlib.sha256(data).hexdigest(),
        "filetype": filetype,
        "dependencies": sorted(set(dylibs)),
        "imports": symbols,
        "objc_classes_imported": classes,
        "objc_selector_refs_resolved": dict(sorted(selector_refs.items())),
        "objc_selector_refs_unresolved": unresolved_refs,
        "objc_method_name_candidates": dict(sorted(names.items())),
        "limitations": [
            "Static import presence is not runtime call frequency.",
            "Objective-C method name strings may be definitions, not referenced selectors.",
            "Chained-fixup-encoded selector pointers unresolved unless raw VM address is recognizable.",
            "Objective-C classes defined locally are not enumerated from class_ro_t."
        ]
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mach_o", nargs="+", type=Path)
    parser.add_argument("--manifest", type=Path,
                        default=Path(__file__).with_name("api_manifest.json"))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        require(manifest.get("schema") == 1 and isinstance(manifest.get("symbols"), dict),
                "invalid implementation manifest")
        reports = []
        for path in args.mach_o:
            require(path.stat().st_size <= MAX_FILE, "Mach-O input exceeds 128 MiB")
            report = analyze(path.read_bytes(), manifest)
            report["input"] = str(path)
            reports.append(report)
        counts = Counter()
        for report in reports:
            counts.update({item["name"]: 1 for item in report["imports"]
                           if item["status"] != "implemented"})
        summary = {
            "schema": 1, "reports": reports,
            "missing_api_prevalence": [{"name": name, "app_count": count}
                                       for name, count in sorted(
                                           counts.items(), key=lambda p: (-p[1], p[0]))]
        }
        output = json.dumps(summary, indent=2, sort_keys=True) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output, encoding="utf-8")
        else:
            print(output, end="")
    except (OSError, ValueError, struct.error) as exc:
        print("gap-report: " + str(exc), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
