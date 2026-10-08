#!/usr/bin/env python3
"""Link original iPhoneOS test binaries with LLVM tools and no Apple SDK.

The local TAPI file declares link metadata only; it provides no runtime.
"""
import argparse
import pathlib
import subprocess
import tempfile


def run(command):
    result = subprocess.run(command, text=True, capture_output=True, timeout=90)
    if result.returncode != 0:
        raise RuntimeError(
            f"{' '.join(map(str, command))} exited {result.returncode}\n"
            f"{result.stdout}\n{result.stderr}"
        )
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang", default="clang-19")
    parser.add_argument("--linker", default="ld64.lld-19")
    parser.add_argument("--inspector", required=True)
    parser.add_argument("--output-dir", default=None)
    args = parser.parse_args()

    repository = pathlib.Path(__file__).resolve().parents[1]
    fixtures = repository / "tests" / "fixtures"
    inspector = pathlib.Path(args.inspector).resolve()
    if not inspector.is_file():
        raise RuntimeError(f"AnyiOS inspector does not exist: {inspector}")

    with tempfile.TemporaryDirectory(prefix="anyios-sdkless-") as tmp:
        work = pathlib.Path(tmp)
        stub = work / "libSystem.tbd"
        stub.write_text(
            "--- !tapi-tbd\n"
            "tbd-version: 4\n"
            "targets: [ arm64-ios ]\n"
            "install-name: '/usr/lib/libSystem.B.dylib'\n"
            "current-version: 1.0\n"
            "...\n", encoding="utf-8"
        )
        clang_flags = [
            args.clang, "--target=arm64-apple-ios15.0", "-O2",
            "-ffreestanding", "-fno-builtin", "-c",
        ]
        app_obj = work / "RuntimeApp.o"
        lib_obj = work / "RuntimeWidget.o"
        run(clang_flags + [str(fixtures / "arm64_linked_app.c"), "-o", str(app_obj)])
        run(clang_flags + [str(fixtures / "arm64_widget.c"), "-o", str(lib_obj)])
        dylib = work / "libRuntimeWidget.dylib"
        app = work / "RuntimeApp"
        link = [args.linker, "-arch", "arm64", "-platform_version", "ios", "15.0", "15.0"]
        run(link + [
            "-dylib", str(lib_obj), "-install_name",
            "@rpath/libRuntimeWidget.dylib", "-L", str(work),
            "-lSystem", "-o", str(dylib),
        ])
        run(link + [
            "-execute", str(app_obj), "-e", "_main",
            "-L", str(work), "-lRuntimeWidget", "-lSystem",
            "-rpath", "@executable_path/Frameworks", "-o", str(app),
        ])
        exe = run([str(inspector), str(app)])
        dep = run([str(inspector), str(dylib)])
        checks = [
            ("File type: 2", exe),
            ("Dylib: @rpath/libRuntimeWidget.dylib", exe),
            ("File type: 6", dep),
            ("Install name: @rpath/libRuntimeWidget.dylib", dep),
        ]
        for expected, output in checks:
            if expected not in output:
                raise AssertionError(f"Missing {expected} from inspector output:\n{output}")
        if args.output_dir:
            import shutil
            output = pathlib.Path(args.output_dir).resolve()
            output.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(app, output / "RuntimeApp")
            shutil.copyfile(dylib, output / "libRuntimeWidget.dylib")
        print("SDK-free LLVM linked project-owned iPhoneOS executable and dylib")
        print("Metadata-only libSystem stub does not provide executable OS services")


if __name__ == "__main__":
    main()
