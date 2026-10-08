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
