import argparse
import pathlib
import shutil
import struct
import subprocess
import tempfile


def run(args):
    result = subprocess.run(args, capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise AssertionError(f"{args[0]} exited {result.returncode}: {result.stderr}")
    return result.stdout


def main():
    arguments = argparse.ArgumentParser()
    arguments.add_argument("inspector")
    arguments.add_argument("--clang", default="clang")
    args = arguments.parse_args()
    with tempfile.TemporaryDirectory(prefix="anyios-owned-macho-") as temporary:
        root = pathlib.Path(temporary)
        source = root / "sample.c"
        source.write_text("int anyios_answer(void) { return 42; }\n", encoding="utf-8")
        arm = root / "arm64.o"
        x86 = root / "x86_64.o"
        run([args.clang, "-target", "arm64-apple-ios13.0", "-c", str(source), "-o", str(arm)])
        run([args.clang, "-target", "x86_64-apple-macos10.15", "-c", str(source), "-o", str(x86)])
        result = run([args.inspector, str(arm)])
        assert "File type: 1" in result, result
        assert "Platform 2: min 13.0.0" in result, result
        assert "Symbol: _anyios_answer" in result, result
        assert "Section: __TEXT/__text" in result, result
        assert "Container: universal" not in result, result

        witness = next((shutil.which(candidate) for candidate in
                        ("llvm-objdump", "llvm-objdump-21", "llvm-objdump-20",
                         "llvm-objdump-19", "llvm-objdump-18", "llvm-objdump-17")
                        if shutil.which(candidate)), None)
        if witness:
            independent = run([witness, "--macho", "--private-headers", str(arm)])
            assert "LC_SEGMENT_64" in independent, independent
            assert "ARM64" in independent, independent

        arm_bytes = arm.read_bytes()
        x86_bytes = x86.read_bytes()
        first = 4096
        second = 8192
        assert len(x86_bytes) < second - first and len(arm_bytes) < second - first
        fat = bytearray(second + len(arm_bytes))
        struct.pack_into(">II", fat, 0, 0xCAFEBABE, 2)
        struct.pack_into(">IIIII", fat, 8, 0x01000007, 3, first, len(x86_bytes), 12)
        struct.pack_into(">IIIII", fat, 28, 0x0100000C, 0, second, len(arm_bytes), 12)
        fat[first:first + len(x86_bytes)] = x86_bytes
        fat[second:second + len(arm_bytes)] = arm_bytes
        universal = root / "universal.bin"
        universal.write_bytes(fat)
        result = run([args.inspector, str(universal)])
        assert "Container: universal (2 architectures)" in result, result
        assert "Platform 2: min 13.0.0" in result, result
        assert f"selected offset={second}" in result, result
        print("Independent Clang Mach-O + universal-container integration passed")


if __name__ == "__main__":
    main()
