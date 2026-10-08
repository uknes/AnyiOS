# Project state

Updated: 2026-10-08. Source history and GitHub Actions are the primary evidence for implemented behavior.

## Verified implementation

- M0: portable C++20 Mach-O inspector and diagnostics; thin ARM64, command/segment/dependency/version metadata, encryption indicator.
- M1 partial: big-endian and byte-swapped FAT/FAT64 universal container selection of ARM64 slices, with bounds, alignment, overlap and subtype checks.
- M1 partial: inspect LC_DYLD_CHAINED_FIXUPS import descriptors and symbol names, plus LC_DYLD_EXPORTS_TRIE symbol names. This does not perform relocation or fixup application.
- M1 partial: inspect section_64 metadata, relocation ranges, LC_SYMTAB names/types/values and LC_DYSYMTAB indirect symbol table bounds. Indirect symbols are not yet resolved.
- M1 tests: synthetic valid and corrupted fixtures, standalone regression tests, deterministic mutations, and compiler-produced real ARM64 iOS *object file* inspection with independent LLVM comparison where available.
- CI for foundational M1: Linux GCC, Linux Clang, Windows MSVC, macOS Clang, and libFuzzer passed on commit 7749776f1cd06ac73013c0aff0dd85e21bc8ca33.
- New section/symbol parser commit: 671383b82ddf5ab144a87b3c82c4e445f9a817cc; verify CI run https://github.com/uknes/AnyiOS/actions/runs/37708487977 before claiming all jobs passed.
- Verified run: https://github.com/uknes/AnyiOS/actions/runs/37708104543
- Standalone local 10,000-run fuzzing and sanitizer checks also passed. Fuzzing is bounded test evidence, not proof of memory safety.

## Honest limitations

- No loader, guest code execution, dyld link resolution or fixup application.
- No Darwin ABI, Mach IPC, libSystem, Objective-C/Swift runtime, Foundation/UIKit or graphics support.
- No iOS app installation, commercial game compatibility, decryption or signing bypass.
- An ARM64 Mach-O object from Clang is not an MH_EXECUTE iOS application. The independent object fixture validates parser behavior, not app compatibility.
- Chained import/export name extraction is partial metadata interpretation. Modern fixup pointer formats, reexports and full validation remain unfinished.

## Immediate next work

1. Finish LC_DYSYMTAB indirect symbol index resolution and validate other dynamic symbol ranges.
2. Verify chained fixup pointer-format semantics and export-terminal encoding without assuming every iOS version is identical.
3. Introduce a dedicated bundle metadata reader only after choosing a small, validated plist implementation.
4. Build reproducible *owned* MH_EXECUTE/MH_DYLIB test fixtures using a capable toolchain; do not simulate execution success.
5. Review _docs/M2_FEASIBILITY.md before implementing memory mappings.

## Source-of-truth rule

Update this ledger only for tested code or grounded research. Notion tracks tasks but never overrides CI evidence or the actual repository revision.
