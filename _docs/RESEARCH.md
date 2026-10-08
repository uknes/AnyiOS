# Research notes — 2026-10-08

## Primary material

- AnyPS5: https://github.com/boykopovar/AnyPS5
- AnyPS5 architecture: https://github.com/boykopovar/AnyPS5/blob/main/docs/dev/ARCHITECTURE.md
- AnyPS5 tests for bad headers: https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_input_magic.py
- AnyPS5 bounds tests: https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/io/tests/BufferBoundsTests.cpp
- AnyPS5 dynamic strings: https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_string_table_bounds.py
- AnyPS5 CI: https://github.com/boykopovar/AnyPS5/blob/main/.github/workflows/build.yml
- Apple open-source Mach-O format reference: https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/loader.h
- touchHLE: https://github.com/touchHLE/touchHLE
- touchHLE integration tests: https://github.com/spheres0/touchhle/blob/trunk/tests/README.md
- Darling: https://github.com/darlinghq/darling
- PlayCover: https://github.com/PlayCover/PlayCover
- iOS code-signing overview: https://support.apple.com/en-ca/guide/security/sec7c917bf14/web

## Transferable lessons

AnyPS5 separates binary relinking, system library replacements and shader translations. Its regression testing includes strict invalid-header diagnostics, buffer-overrun tests, synthetic malformed import/string tables and CTest/CI. We adopt separation, defensive diagnostics and evidence-based tests, not its source code.

## Non-transferable assumptions

PS5 is ELF/x86-64 with PRX NIDs; iOS is Mach-O/ARM64 with dylib/dyld semantics, Darwin APIs and ObjC/Swift. Linux/Android ARM64 cannot execute an unmodified iOS process automatically. Graphics and modern Metal interoperability are independent large problems.

## Outstanding evidence

Our current binary samples are synthetic. Test using independently built legitimate ARM64 Mach-O executables, compare with LLVM/Apple inspection tools, and distinguish simulator vs device binaries. No claim of iOS app or game execution is warranted.


## M1 references consulted (2026-10-08)

- Apple's FAT wrapper header: https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/fat.h
- LLVM Mach-O structs: https://llvm.org/doxygen/BinaryFormat_2MachO_8h_source.html
- Apple's dyld chained-fixup definitions: https://github.com/apple-oss-distributions/dyld/blob/main/include/mach-o/fixup-chains.h
- Apple dyld validation/reference implementation: https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOAnalyzer.cpp
- Clang cross-compilation guidance: https://clang.llvm.org/docs/CrossCompilation.html
- GNUstep libobjc2: https://github.com/gnustep/libobjc2

An owned iOS ARM64 MH_OBJECT generated with Clang was successfully parsed and compared with LLVM output. Compiling an object does not validate actual app binary loading. GNUstep libobjc2 is a research candidate, not a proven binary-compatible replacement for Apple's iOS Objective-C runtime. Darwin syscall, dyld, Mach IPC, app services and frameworks remain open design problems.

## Windows x86-64 research (2026-10-08)

- Dynarmic core is licensed 0BSD, supports ARM64 guests and Windows x86-64 hosts; pinned revision a46601580d5512d324104f985b5f0209dc980ddc. https://github.com/azahar-emu/dynarmic
- FEX-Emu's usual direction is x86-64 guest to ARM64 host, not ARM64 iOS guest to Windows x86-64 host. https://github.com/FEX-Emu/FEX
- Unicorn Engine's GPLv2 license makes copying into MIT AnyiOS unsuitable without relicensing review. https://github.com/unicorn-engine/unicorn
- GNUstep libobjc2 and the Windows MSVC toolchain provide Objective-C service research targets, but their guest ABI is not automatically compatible with Apple-compiled ARM64 code. https://github.com/gnustep/libobjc2 and https://github.com/gnustep/tools-windows-msvc
- Apple objc4 source uses APSL-2.0; review license before incorporating any code. https://github.com/apple-oss-distributions/objc4

## Real iOS linked-image fixtures — 2026-10-08

Apple's ld64 manual describes MH_EXECUTE, MH_DYLIB, dyld install names and explicitly limits static executables to kernel scenarios. Actual iOS user apps depend on dyld and system frameworks. References:
- https://github.com/apple-oss-distributions/ld64/blob/main/doc/man/man1/ld-classic.1
- https://developer.apple.com/forums/tags/linker
- https://llvm.org/docs/CommandGuide/llvm-objdump.html

Original minimal C sources now provide a dylib and an executable depending on it, compiled and linked with the iPhoneOS Xcode toolchain on a macOS GitHub runner. tests/linked_ios_fixture.py inspects the resulting ARM64 MH_EXECUTE/MH_DYLIB, install name and @rpath metadata. Fixture binaries are built only in CI; no Apple SDK binaries or proprietary runtime blobs are committed.

This verifies actual linker-produced metadata only, not Windows execution. A linked app requires import binding, dynamic linker support and libSystem/API compatibility that AnyiOS has not implemented.
