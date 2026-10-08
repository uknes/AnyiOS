#!/usr/bin/env python3
"""Compile ORIGINAL danqing/2048 Objective-C app entry+delegate for iPhoneOS.

Not the full game: SpriteKit scene/model, UIKit storyboard, and CoreGraphics
are NOT compiled, linked, instantiated or implemented. A metadata-only TAPI
supplies unresolved import names for static Windows guest inspection.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.request

from build_bitrise_probe import FOUNDATION, UIKIT

REPOSITORY = "danqing/2048"
REVISION = "6f89eab8f3e5e044f66c381095a9f5402bacaab5"
FILES = ("m2048/main.m", "m2048/M2AppDelegate.h", "m2048/M2AppDelegate.m")


def fetch(path):
    url = ("https://raw.githubusercontent.com/" + REPOSITORY + "/" +
           REVISION + "/" + path)
    with urllib.request.urlopen(url, timeout=20) as response:
        raw = response.read(262145)
    if len(raw) == 0 or len(raw) > 262144:
        raise ValueError("upstream MIT source file has invalid length")
    return raw


def run(command, cwd):
    p = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=90)
    if p.returncode:
        raise RuntimeError("command failed: " + " ".join(map(str, command)) +
                           "\n" + p.stdout + p.stderr)
    return p.stdout


def build(clang, linker, nm, output):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    result = {
        "repository": REPOSITORY,
        "commit": REVISION,
        "status": "blocked",
        "scope": "original main.m + M2AppDelegate.m ONLY; not playable",
        "frameworks": "not implemented; metadata-only synthetic TAPI",
        "full_ios_app": False,
        "playable": False,
        "phase": "download"
    }
    try:
        with tempfile.TemporaryDirectory(prefix="anyios-mit-2048-") as tmp:
            root = Path(tmp)
            license_bytes = fetch("LICENSE")
            if b"The MIT License (MIT)" not in license_bytes or b"Danqing Liu" not in license_bytes:
                raise ValueError("upstream 2048 MIT license contents changed")
            result["license"] = "MIT (actual file checked at pinned revision)"
            result["license_sha256"] = hashlib.sha256(license_bytes).hexdigest()
            source = root / "src"
            source.mkdir()
            for path in FILES:
                (source / Path(path).name).write_bytes(fetch(path))
            headers = root / "include"
            (headers / "Foundation").mkdir(parents=True)
            (headers / "UIKit").mkdir(parents=True)
            (headers / "Foundation" / "Foundation.h").write_text(FOUNDATION, encoding="utf-8")
            (headers / "UIKit" / "UIKit.h").write_text(UIKIT, encoding="utf-8")
            result["phase"] = "compile"
            objects = []
            for filename in ("main.m", "M2AppDelegate.m"):
                obj = root / (filename + ".o")
                run([clang, "--target=arm64-apple-ios15.0", "-fobjc-runtime=ios-15.0",
                     "-fno-objc-arc", "-fno-stack-protector",
                     "-Wno-incompatible-pointer-types", "-Wno-objc-root-class",
                     "-Wno-objc-missing-property-synthesis",
                     "-I", str(headers), "-I", str(source),
                     "-c", str(source / filename), "-o", str(obj)], root)
                objects.append(obj)
            result["phase"] = "link"
            output_nm = run([nm, "-u"] + [str(obj) for obj in objects], root)
            imports = sorted(set(re.findall(r"\b(_[A-Za-z0-9_$\.]+)\s*$",
                                            output_nm, re.M)))
            if not imports or "_UIApplicationMain" not in imports:
                raise ValueError("upstream 2048 UIApplicationMain import not present")
            result["metadata_only_imports"] = imports
            symbols = ", ".join("'" + s + "'" for s in imports)
            stub = root / "libAnyiOSUnimplemented.tbd"
            stub.write_text(
                "--- !tapi-tbd\n"
                "tbd-version: 4\n"
                "targets: [ arm64-ios ]\n"
                "install-name: '/usr/lib/libAnyiOSUnimplemented.dylib'\n"
                "exports:\n"
                "  - targets: [ arm64-ios ]\n"
                "    symbols: [ " + symbols + " ]\n"
                "...\n", encoding="utf-8")
            binary = output / "Danqing2048Entry"
            run([linker, "-arch", "arm64", "-platform_version", "ios",
                 "15.0", "15.0", "-seg_page_size", "__TEXT", "0x4000",
                 "-seg_page_size", "__DATA", "0x4000",
                 "-seg_page_size", "__DATA_CONST", "0x4000",
                 "-seg_page_size", "__LINKEDIT", "0x4000",
                 "-execute"] + [str(obj) for obj in objects] +
                ["-e", "_main", "-L", str(root), "-lAnyiOSUnimplemented",
                 "-o", str(binary)], root)
            result["binary_sha256"] = hashlib.sha256(binary.read_bytes()).hexdigest()
            result["phase"] = "complete"
            result["status"] = "original-objc-entry-linked-only"
            print("Original MIT Danqing 2048 iOS Objective-C entry and delegate linked")
            print("NOT the SpriteKit game; NO UIKit runtime; NOT playable")
    except (OSError, RuntimeError, ValueError) as error:
        result["error"] = str(error)[-14000:]
        print("Danqing 2048 upstream compile probe blocked: " + str(error)[-1200:],
              file=sys.stderr)
    (output / "danqing-2048-status.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return result["status"]


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--clang", default="clang-19")
    ap.add_argument("--linker", default="ld64.lld-19")
    ap.add_argument("--nm", default="llvm-nm-19")
    ap.add_argument("--output-dir", type=Path, required=True)
    a = ap.parse_args()
    print("Danqing 2048 compatibility status:", build(a.clang, a.linker, a.nm,
                                                      a.output_dir))
