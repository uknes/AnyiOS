# Roadmap — evidence-based acceptance

## M0/M1 — Binary format and security foundations

- [x] Thin ARM64 Mach-O, FAT/FAT64, sections, imports, versions, dylib metadata
- [x] Synthetic negative fixtures, compiler-generated MH_OBJECT comparisons and libFuzzer
- [x] Mach-O `LC_ID_DYLIB` and typed weak/reexport/upward dependency metadata (implementation committed; CI verification pending)
- [ ] Full chained-fixup pointer formats, rebases, bindings, export terminal validation
- [ ] App bundle Info.plist, executable path and resource manifests

## M2 — Windows x64 translation and restricted execution

- [x] Dynarmic JIT executes a compiler-generated iOS ARM64 Mach-O function on Windows x64
- [x] Bounded guest address space and W^X runtime writes
- [x] Synthetic MH_EXECUTE mapping and guarded code execution
- [ ] Verify Darwin SVC guest write/exit bridge on Windows x64 in GitHub CI
- [ ] Verify deterministic bundle dependency planner on all supported platforms
- [ ] Link owned MH_EXECUTE/MH_DYLIB fixtures independently and execute via a real loader
- [ ] Import resolution, chained fixups and relocation rollback with owned dependencies
- [ ] Darwin x16 syscall ABI, guest stack/thread state and first libSystem subset

## M3 — iOS runtime / Framework HLE

- [ ] Mach ports and event-loop contracts, thread-local state and dispatch
- [ ] Objective-C runtime and Foundation/CoreFoundation contract tests
- [ ] Basic UIKit/CoreAnimation window and input
- [ ] Graphics backend selection and verified feature requirements

## M4 — Own real app compatibility

- [ ] Launch an unprotected, redistributable developer-owned `.app` on Windows x86-64
- [ ] App lifecycle, window events, input and at least one rendered frame
- [ ] Per-app compatibility matrix and explicit unsupported API reports

Commercial games, protected App Store IPAs, arm64e and modern Metal support are **not promised**. Do not label a JIT or Mach-O parser test an app launch.