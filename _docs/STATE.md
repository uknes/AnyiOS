# Project state

Updated: 2026-10-08. Git commits and CI are authoritative; this document records verified engineering state.

## Verified M0 foundation

- C++20 thin, little-endian ARM64 Mach-O inspector library and CLI.
- Reads header, load-command boundaries, segments, dylib paths, runpaths, LC_MAIN, version metadata, and encryption indicators.
- Synthetic positive/negative fixtures and 3,000 deterministic input mutations.
- Local GCC build: both CTests passed.
- Local Clang AddressSanitizer/UndefinedBehaviorSanitizer build: both CTests passed.
- GitHub Actions run for commit 83c25dab00f98e63578d46ce3e78be8cd378aaf2: Linux GCC, Linux Clang, Windows MSVC, macOS Clang **all passed**.
- CI evidence: https://github.com/uknes/AnyiOS/actions/runs/37706817295

## Not implemented

No guest application execution, loader mapping, dyld, symbol resolution, Darwin ABI, Objective-C/Swift runtimes, Foundation, UIKit, graphics, fat Mach-O, IPA installation, or real iOS game compatibility. No encryption circumvention.

## Next milestone

1. Build independent owned ARM64 Mach-O fixtures and compare output to LLVM/Apple tooling.
2. Add bounds-checked fat/universal container and dyld fixup/export metadata inspection.
3. Develop coverage-guided fuzz corpus and run it in CI.
4. Research Darwin ARM64/Linux process ABI and runtime design before executing any guest code.

## State integrity rule

Update this file only when there is an actual code revision, reproducible evidence, or a clearly identified research outcome. A new documentation-only revision may trigger a new CI run; the verified run above refers specifically to the implementation commit shown.