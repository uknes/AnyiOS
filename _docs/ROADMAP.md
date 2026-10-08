# Milestones and acceptance

## M0 — Portable passive Mach-O inspector

- [x] Thin little-endian ARM64 Mach-O metadata
- [x] Load commands, segments, dependencies, rpaths, entry, versions, encrypted-code flags
- [x] Negative regression fixtures and deterministic mutation tests
- [x] Cross-platform CI and local Clang sanitizer checks

## M1 — Expanded static image analysis (in progress)

- [x] Safely locate ARM64 in FAT/FAT64 and swapped-endian universal containers
- [x] Inspect chained dyld import-name metadata and exported-symbol trie names
- [x] Test with Clang-built owned iOS ARM64 object files; independent LLVM verification when present
- [x] LLVM libFuzzer with ASan/UBSan, original synthetic corpus and CI run
- [x] Explicit count, symbol length, path traversal and aggregate allocation limits
- [ ] Inspect sections, symbol tables and indirect symbol ranges
- [ ] Validate full chained pointer formats, export terminal variants and import ordinals
- [ ] Inspect original app bundle metadata and Info.plist
- [ ] Obtain reproducible independently linked owned MH_EXECUTE and MH_DYLIB fixtures

## M2 — Controlled owned code execution (not started)

- [ ] Document ARM64 host/guest calling conventions, Darwin syscalls and Mach facilities
- [ ] Define page-mapping and relocation contracts with executable security isolation
- [ ] Prove an owned hello-world MH_EXECUTE can be correctly loaded and called
- [ ] Introduce precise unsupported-API diagnostics

## M3 — Runtime contracts (not started)

- [ ] Test libSystem/Darwin APIs against owned samples
- [ ] Evaluate ObjC/Swift ABI requirements and license compatibility

## M4 — App lifecycle (not started)

- [ ] Owned iOS sample window/input/bootstrap with reproducible evidence
- [ ] Versioned support matrix and negative API tests

Not promised: retail decrypted content, Apple services, ARM64e/PAC support, Metal translation, arbitrary commercial applications or x86 execution.
