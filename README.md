# AnyiOS

Research into a clean-room compatibility layer for legally accessible, unprotected ARM64 iOS application binaries on non-Apple operating systems.

**Current status: Mach-O inspection only.** This project cannot launch iOS applications. It does not implement Darwin system APIs, Apple frameworks, decryption, signing, or an iOS emulator.

## Build and test

Requires CMake 3.20+ and a C++20 compiler (Linux GCC/Clang, Windows MSVC, and macOS Clang are tested).

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --config Debug --parallel
    ctest --test-dir build --build-config Debug --output-on-failure

## Inspector

    ./build/anyios-inspect path/to/owned-unprotected-arm64-macho

On Windows use the generated anyios-inspect executable inside the configured build directory. The inspector accepts thin little-endian ARM64 Mach-O and ARM64 slices in FAT/FAT64 universal binaries (including swapped-endian tables). It also lists section_64 and LC_SYMTAB metadata, dyld chained-import names, and export-trie names. Other CPUs and malformed inputs are rejected. Exit codes: 0 valid unprotected metadata, 1 input/usage error, 2 unsupported/malformed format, 3 encrypted-code indicator. Inspecting a file does not mean it is executable or is an iOS device application.

## Real compiler fixture and fuzzing

On Linux with Clang and Python 3 installed:

    python3 tests/real_fixture.py build/anyios-inspect

To build the dedicated Clang/libFuzzer test target:

    cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DANYIOS_BUILD_FUZZER=ON -DBUILD_TESTING=OFF
    cmake --build build-fuzz --target anyios-fuzz
    python3 tests/fuzz_corpus.py build-fuzz/corpus
    ./build-fuzz/anyios-fuzz build-fuzz/corpus -runs=10000 -max_len=4096

Neither inspection nor successful fuzzing constitutes guest-code execution.

## Project documentation

- [_docs/STATE.md](_docs/STATE.md) — verified state and next milestone
- [_docs/ARCHITECTURE.md](_docs/ARCHITECTURE.md) — implementation boundaries
- [_docs/M2_FEASIBILITY.md](_docs/M2_FEASIBILITY.md) — evidence-based execution feasibility
- [_docs/ROADMAP.md](_docs/ROADMAP.md) — acceptance-based phases
- [_docs/DECISIONS.md](_docs/DECISIONS.md) — architectural decisions
- [_docs/RESEARCH.md](_docs/RESEARCH.md) — research, prior art and sources
- [_docs/TESTING.md](_docs/TESTING.md) — test method and requirements
- [_docs/SECURITY.md](_docs/SECURITY.md) — threat model and lawful-input policy

## Compatibility and licensing

Only project-owned or explicitly redistributable unprotected binaries should be used. No firmware, cryptographic keys, protected retail apps, DRM-bypass tooling, or proprietary Apple binaries are provided. Source is licensed under MIT. We studied [AnyPS5](https://github.com/boykopovar/AnyPS5) for engineering patterns without copying its GPL-licensed code.

Verified M1 GitHub CI (GCC, Clang, MSVC, macOS, and sanitizer-enabled fuzzing): https://github.com/uknes/AnyiOS/actions/runs/37708104543