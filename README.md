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

On Windows use the generated anyios-inspect executable inside the configured build directory. The inspector examines a thin little-endian ARM64 Mach-O; fat/universal binaries and unsupported architectures are currently rejected. Exit codes: 0 valid unprotected metadata, 1 input/usage error, 2 unsupported/malformed format, 3 encrypted-code indicator. Inspecting a file does not mean it is executable or is an iOS device application.

## Project documentation

- [_docs/STATE.md](_docs/STATE.md) — verified state and next milestone
- [_docs/ARCHITECTURE.md](_docs/ARCHITECTURE.md) — implementation boundaries
- [_docs/ROADMAP.md](_docs/ROADMAP.md) — acceptance-based phases
- [_docs/DECISIONS.md](_docs/DECISIONS.md) — architectural decisions
- [_docs/RESEARCH.md](_docs/RESEARCH.md) — research, prior art and sources
- [_docs/TESTING.md](_docs/TESTING.md) — test method and requirements
- [_docs/SECURITY.md](_docs/SECURITY.md) — threat model and lawful-input policy

## Compatibility and licensing

Only project-owned or explicitly redistributable unprotected binaries should be used. No firmware, cryptographic keys, protected retail apps, DRM-bypass tooling, or proprietary Apple binaries are provided. Source is licensed under MIT. We studied [AnyPS5](https://github.com/boykopovar/AnyPS5) for engineering patterns without copying its GPL-licensed code.

Initial GitHub CI evidence: https://github.com/uknes/AnyiOS/actions/runs/37706817295