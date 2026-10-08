# Project state

Updated: 2026-10-08. Git commits and CI are authoritative; this is a human-readable ledger.

## Verified locally

- C++20 ARM64 thin little-endian Mach-O inspector library and CLI.
- Parses header, segment ranges, load-command boundaries, dylib, rpath, entry, version and encrypted-code flag.
- Unit tests include negative fixture cases and 3,000 deterministic mutations.
- GCC 14.2: 2/2 CTest checks passed.
- Clang AddressSanitizer + UndefinedBehaviorSanitizer: 2/2 checks passed.
- GitHub CI matrix is configured, but remote verification is pending.

## Not implemented

Execution, loader, dylib resolution, dyld, Darwin ABI, ObjC/Swift, Foundation, UIKit, graphics, fat Mach-O, IPA installation and commercial game support.

## Next

1. Observe remote CI and fix any actual failures.
2. Generate independent owned ARM64 Mach-O samples and compare against LLVM/Apple tooling.
3. Add fat format and dyld fixup/exports inspection.
4. Write an ARM64/Linux execution feasibility document before running guest code.

## M0 acceptance

Correct original fixtures; predictable malformed-input diagnostics; no guest execution; Linux and another platform's CI passing.
