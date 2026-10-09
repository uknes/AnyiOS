"""Exercise actual filesystem reads and failure paths of the metadata-only probe."""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def macho(kind, commands):
    payload = b""
    for code, name in commands:
        size = (24 + len(name) + 1 + 7) & ~7
        payload += struct.pack("<IIIIII", code, size, 24, 0, 0, 0) + name.encode() + b"\0"
        payload += bytes(size - 24 - len(name) - 1)
    return struct.pack("<IIIIIIII", 0xfeedfacf, 0x100000c, 0, kind, len(commands), len(payload), 0, 0) + payload


def main(probe):
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp) / "Original.app"
        root.mkdir()
        main_file = root / "App"
        dylib = root / "App.debug.dylib"
        main_file.write_bytes(macho(2, [(0x8000001c, "/usr/lib/swift"), (0x8000001c, "@executable_path"), (0xc, "@rpath/App.debug.dylib")]))
        dylib.write_bytes(macho(6, [(0xd, "@rpath/App.debug.dylib"), (0xc, "/System/Library/Frameworks/UIKit.framework/UIKit")]))
        before = [hashlib.sha256(path.read_bytes()).hexdigest() for path in (main_file, dylib)]

        def run(entry="App"):
            return subprocess.run([probe, str(root), entry], text=True, capture_output=True, check=False)

        result = run()
        assert result.returncode == 0, result.stderr
        assert "BUNDLE_EXTERNAL_RPATH=/usr/lib/swift" in result.stdout
        assert "BUNDLE_MODULE_RPATH=/usr/lib/swift" in result.stdout
        assert "BUNDLE_MODULE_COUNT=2" in result.stdout
        assert "BUNDLE_UNRESOLVED_LOADER=Bundle/App.debug.dylib" in result.stdout
        assert "BUNDLE_DEPENDENCY_CLOSURE=incomplete" in result.stdout
        assert "BUNDLE_STAGING=not-attempted" in result.stdout
        assert before == [hashlib.sha256(path.read_bytes()).hexdigest() for path in (main_file, dylib)]
        assert run("../outside").returncode == 3
        original = dylib.read_bytes()
        dylib.write_bytes(b"malformed-Mach-O")
        assert run().returncode == 3
        dylib.unlink()
        result = run()
        assert result.returncode == 0 and "BUNDLE_UNRESOLVED_DEPENDENCY=@rpath/App.debug.dylib" in result.stdout
        dylib.mkdir()
        assert run().returncode == 3
        dylib.rmdir()
        outside = Path(temp) / "outside.dylib"
        outside.write_bytes(original)
        try:
            dylib.symlink_to(outside)
        except OSError:
            print("SYMLINK_TEST=unavailable-on-host")
        else:
            assert run().returncode == 3
            dylib.unlink()
        with dylib.open("wb") as file:
            file.truncate(128 * 1024 * 1024 + 1)
        assert run().returncode == 3
    print("Bounded bundle metadata filesystem contracts passed")


if __name__ == "__main__":
    main(sys.argv[1])
