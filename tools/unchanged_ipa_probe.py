#!/usr/bin/env python3
"""Read-only original iOS application bundle/IPA interoperability intake.

Does NOT modify the iOS program, resign an IPA, bypass FairPlay, fetch App Store
code, emulate UIKit, or claim to have launched the game. This merely inspects
a caller-provided legally usable, unprotected ARM64 iOS binary in memory.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import struct
import sys
import zipfile

import macho_gap

MAX_MACHO = 128 * 1024 * 1024
MAX_PLIST = 1024 * 1024
MAX_ARCHIVE_SIZE = 2 * 1024 * 1024 * 1024
MAX_ENTRIES = 20000
LC_ENCRYPTION_INFO = 0x21
LC_ENCRYPTION_INFO_64 = 0x2c


class IntakeError(ValueError):
    pass


def file_contents(path, limit):
    if not path.is_file() or path.is_symlink():
        raise IntakeError("expected ordinary on-disk file (no symbolic links)")
    if path.stat().st_size > limit:
        raise IntakeError("input file exceeds safe read bound")
    return path.read_bytes()


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def encryption_info(data):
    """Strictly extract cryptid from ARM64 Mach-O slice; no decryption."""
    try:
        blob = macho_gap.arm64_slice(data)
    except macho_gap.InvalidMachO as error:
        raise IntakeError(str(error)) from error
    if len(blob) < 32:
        raise IntakeError("truncated ARM64 header")
    magic, cpu, sub, typ, count, size, flags, reserved = struct.unpack_from("<8I", blob, 0)
    if magic != 0xfeedfacf or cpu != macho_gap.ARM64:
        raise IntakeError("bundle executable is not ARM64 Mach-O")
    if count > 4096 or size > len(blob) - 32:
        raise IntakeError("invalid Mach-O load commands")
    position = 32
    end = position + size
    encrypted = []
    for _ in range(count):
        if position + 8 > end:
            raise IntakeError("truncated load command")
        command, amount = struct.unpack_from("<II", blob, position)
        if amount < 8 or amount > end - position or amount % 4:
            raise IntakeError("invalid load-command size")
        if command in (LC_ENCRYPTION_INFO, LC_ENCRYPTION_INFO_64):
            minimum = 24 if command == LC_ENCRYPTION_INFO_64 else 20
            if amount < minimum:
                raise IntakeError("truncated encryption command")
            off, length, cryptid = struct.unpack_from("<III", blob, position + 8)
            if length and (off > len(blob) or length > len(blob) - off):
                raise IntakeError("invalid encrypted section range")
            encrypted.append({"cryptid": cryptid, "cryptoff": off, "cryptsize": length})
        position += amount
    if position != end:
        raise IntakeError("bad Mach-O load command count")
    return {"encrypted": any(e["cryptid"] != 0 for e in encrypted),
            "encryption_commands": encrypted}


def validate_bundle_name(name):
    if not isinstance(name, str) or not name or len(name) > 255:
        raise IntakeError("invalid CFBundleExecutable")
    if name in (".", "..") or "/" in name or "\\" in name or "\x00" in name:
        raise IntakeError("unsafe CFBundleExecutable path")
    return name


def read_ipa(path):
    with zipfile.ZipFile(path) as archive:
        files = archive.infolist()
        if len(files) > MAX_ENTRIES:
            raise IntakeError("too many IPA entries")
        if sum(i.file_size for i in files) > MAX_ARCHIVE_SIZE:
            raise IntakeError("oversized IPA uncompressed payload")
        if any(i.file_size > max(1024, i.compress_size) * 200 for i in files):
            raise IntakeError("suspicious IPA ZIP compression ratio")
        roots = sorted({
            match.group(1) for info in files
            if (match := re.fullmatch(r"Payload/([^/]+[.]app)/Info[.]plist", info.filename))
        })
        if len(roots) != 1:
            raise IntakeError("expected exactly one top-level Payload/*.app/Info.plist")
        root = f"Payload/{roots[0]}/"
        plist_info = archive.getinfo(root + "Info.plist")
        if plist_info.file_size > MAX_PLIST:
            raise IntakeError("oversized Info.plist")
        info = plistlib.loads(archive.read(plist_info))
        name = validate_bundle_name(info.get("CFBundleExecutable"))
        binary_info = archive.getinfo(root + name)
        if binary_info.file_size > MAX_MACHO:
            raise IntakeError("iOS binary exceeds safe 128 MiB inspection bound")
        binary = archive.read(binary_info)
        # Archive is never extracted or modified: immutable, original bytes only.
        return info, binary, f"{path}!/{root}{name}"


def read_bundle(path):
    if path.suffix.lower() == ".ipa" and path.is_file():
        if path.stat().st_size > MAX_ARCHIVE_SIZE:
            raise IntakeError("IPA archive exceeds safe read bound")
        return read_ipa(path)
    if path.suffix.lower() == ".app" and path.is_dir():
        info = plistlib.loads(file_contents(path / "Info.plist", MAX_PLIST))
        exe = validate_bundle_name(info.get("CFBundleExecutable"))
        return info, file_contents(path / exe, MAX_MACHO), str(path / exe)
    if path.is_file():
        return {}, file_contents(path, MAX_MACHO), str(path)
    raise IntakeError("supply an original unmodified .ipa, .app directory, or ARM64 Mach-O")


def examine(path, manifest):
    info, data, location = read_bundle(path)
    security = encryption_info(data)
    result = {
        "binary_location": location,
        "bundle_id": info.get("CFBundleIdentifier"),
        "bundle_name": info.get("CFBundleDisplayName", info.get("CFBundleName")),
        "binary_sha256": sha256(data),
        "binary_size": len(data),
        "architecture": "arm64-ios",
        "original_bytes_modified": False,
        "execution_attempted": False,
        "window_created": False,
        "input_tested": False,
        **security,
    }
    if security["encrypted"]:
        result["status"] = "blocked-protected-macho"
        result["reason"] = "Encrypted LC_ENCRYPTION_INFO_64/LC_ENCRYPTION_INFO: no DRM decryption support"
        return result
    try:
        report = macho_gap.analyze(data, manifest)
        result["status"] = "static-imports-inspected-only"
        result["imports"] = report["imports"]
        result["unverified_imports"] = [
            x["name"] for x in report["imports"] if x["status"] != "implemented"
        ]
        result["framework_dependencies"] = report["dependencies"]
        result["objc_classes_imported"] = report["objc_classes_imported"]
        result["api_candidates"] = len(report["imports"])
    except (macho_gap.InvalidMachO, struct.error) as error:
        result["status"] = "blocked-unsupported-macho-metadata"
        result["reason"] = str(error)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, nargs="?",
                        help="legally usable unmodified .ipa, .app directory or Mach-O")
    parser.add_argument("--target", default="sneaky-sasquatch",
                        choices=["sneaky-sasquatch"])
    parser.add_argument("--output", type=Path)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    target = json.loads((root / "compatibility/targets" /
                         (options.target + ".json")).read_text(encoding="utf-8"))
    report = {"schema": 1, "target": options.target, "title": target["name"],
              "developer": target["developer"],
              "original_application": True,
              "original_bytes_modified": False,
              "execution_attempted": False, "window_created": False,
              "source_code_changes": False}
    if options.input is None:
        report["status"] = "blocked-no-original-app-binary"
        report["reason"] = (
            "No legally acquired, unprotected original Apple Arcade app "
            "executable was provided; no fake or substituted binary was used."
        )
        code = 2
    else:
        try:
            manifest = json.loads((root / "tools/api_manifest.json").read_text("utf-8"))
            report.update(examine(options.input, manifest))
            code = 0 if report["status"] == "static-imports-inspected-only" else 2
        except (OSError, IntakeError, zipfile.BadZipFile, ValueError,
                plistlib.InvalidFileException) as error:
            report["status"] = "blocked-malformed-or-unavailable-original-app"
            report["reason"] = str(error)
            code = 2
    result = json.dumps(report, sort_keys=True, indent=2) + "\n"
    if options.output:
        options.output.parent.mkdir(parents=True, exist_ok=True)
        options.output.write_text(result, encoding="utf-8")
    print(result, end="")
    return code


if __name__ == "__main__":
    sys.exit(main())
