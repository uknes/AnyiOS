#!/usr/bin/env python3
"""Link the project-owned interactive 2048 ARM64 iPhoneOS guest without any Apple SDK.

This is deliberately NOT danqing/2048. It establishes Windows GDI input/render
and Dynarmic guest execution before attempting the original SpriteKit game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

def run(args):
    result = subprocess.run([str(a) for a in args], text=True, capture_output=True, timeout=120)
    if result.returncode != 0:
        raise RuntimeError("build failed: " + " ".join(map(str, args)) +
                           "\n" + result.stdout + result.stderr)
    return result.stdout

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang", default="clang-19")
    parser.add_argument("--linker", default="ld64.lld-19")
    parser.add_argument("--nm", default="llvm-nm-19")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--inspector", type=Path)
    opts = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = opts.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="anyios-guest-ui-") as tmp:
        obj = Path(tmp) / "Anyios2048.o"
        run([opts.clang, "--target=arm64-apple-ios15.0", "-O1",
             "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
             "-fvisibility=default", "-c",
             root / "tests/fixtures/arm64_2048_ui_guest.c", "-o", obj])
        binary = out / "AnyiOS2048Guest"
        run([opts.linker, "-arch", "arm64", "-platform_version", "ios", "15.0", "15.0",
             "-seg_page_size", "__TEXT", "0x4000",
             "-seg_page_size", "__DATA", "0x4000",
             "-seg_page_size", "__DATA_CONST", "0x4000",
             "-seg_page_size", "__LINKEDIT", "0x4000",
             "-execute", obj, "-e", "_main",
             "-exported_symbol", "_anyios_2048_state",
             "-exported_symbol", "_anyios_2048_move",
             "-exported_symbol", "_anyios_2048_reset",
             "-o", binary])
        symbols = run([opts.nm, str(binary)])
        for name in ("_main", "_anyios_2048_state",
                     "_anyios_2048_move", "_anyios_2048_reset"):
            if name not in symbols:
                raise AssertionError("linked guest missing original symbol " + name)
        if opts.inspector:
            info = run([opts.inspector.resolve(), binary])
            for required in ("File type: 2", "Entry file offset:"):
                if required not in info:
                    raise AssertionError("owned iOS executable invalid: " + required)
        receipt = {
            "status": "linked-owned-ios-arm64-guest",
            "source": "AnyiOS original C guest fixture (NOT the MIT iOS 2048 application)",
            "licensed_upstream_game": False,
            "architecture": "arm64-apple-ios15.0",
            "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            "symbols": ["_main", "_anyios_2048_state",
                        "_anyios_2048_move", "_anyios_2048_reset"],
            "framework_imports": [],
            "gui_native_host": "Windows GDI",
            "full_ios_app": False
        }
        (out / "guest-receipt.json").write_text(
            json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        print("Linked owned iPhoneOS ARM64 2048 game-state guest; NOT external iOS 2048")
        print("Guest sha256:", receipt["sha256"])

if __name__ == "__main__":
    main()
