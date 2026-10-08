import pathlib
import shutil
import subprocess
import sys
import tempfile


def run(args):
    process = subprocess.run(args, text=True, capture_output=True, timeout=60)
    if process.returncode:
        raise AssertionError(f"command failed: {args}\n{process.stdout}\n{process.stderr}")
    return process.stdout


def main():
    inspector = pathlib.Path(sys.argv[1]).resolve()
    if not inspector.is_file():
        raise AssertionError(f"missing AnyiOS inspector: {inspector}")
    if not shutil.which("xcrun"):
        raise AssertionError("macOS Xcode toolchain required for linked iOS fixture")
    sdk = run(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"]).strip()
    if not pathlib.Path(sdk).is_dir():
        raise AssertionError("iPhoneOS SDK unavailable on this build host")
    root = pathlib.Path(__file__).resolve().parent / "fixtures"
    with tempfile.TemporaryDirectory(prefix="anyios-ios-link-") as temporary:
        output = pathlib.Path(temporary)
        dylib = output / "libWidget.dylib"
        executable = output / "SampleApp"
        common = ["xcrun", "--sdk", "iphoneos", "clang",
                  "--target=arm64-apple-ios13.0", "-isysroot", sdk, "-arch", "arm64"]
        run(common + ["-dynamiclib", "-Wl,-install_name,@rpath/libWidget.dylib",
                      str(root / "arm64_widget.c"), "-o", str(dylib)])
        run(common + [str(root / "arm64_linked_app.c"), "-L", str(output), "-lWidget",
                      "-Wl,-rpath,@executable_path/Frameworks",
                      "-o", str(executable)])
        app_text = run([str(inspector), str(executable)])
        lib_text = run([str(inspector), str(dylib)])
        assert "Format: Mach-O 64-bit ARM64" in app_text, app_text
        assert "File type: 2" in app_text, app_text
        assert "Dylib: @rpath/libWidget.dylib" in app_text, app_text
        assert "Rpath: @executable_path/Frameworks" in app_text, app_text
        assert "Platform 2:" in app_text, app_text
        assert "File type: 6" in lib_text, lib_text
        assert "Install name: @rpath/libWidget.dylib" in lib_text, lib_text
        if len(sys.argv) >= 3:
            destination = pathlib.Path(sys.argv[2]).resolve()
            destination.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(executable, destination / "SampleApp")
            shutil.copyfile(dylib, destination / "libWidget.dylib")
            print(f"Saved independently linked project-owned iOS binaries to {destination}")
        print("Linked iOS MH_EXECUTE + MH_DYLIB and dyld install-name metadata verified")
        print("Inspection only: dependent executable still requires dyld and libSystem emulation")


if __name__ == "__main__":
    main()
