# Roadmap — parallel Windows x64 and ARM64 host tracks

## M0/M1 — Binary format and security foundations

- [x] Thin ARM64 Mach-O, FAT/FAT64, sections, imports, versions, dylib metadata
- [x] Synthetic negative fixtures, compiler-generated MH_OBJECT comparisons and libFuzzer
- [x] Mach-O `LC_ID_DYLIB` and typed weak/reexport/upward dependency metadata (implementation committed; CI verification pending)
- [ ] Full chained-fixup pointer formats, rebases, bindings, export terminal validation
- [x] Restricted generic-64 chained bind/rebase planner and exact two-level nlist resolver passed cross-platform CI (run 37751343388)
- [ ] App bundle Info.plist, executable path and resource manifests

## M2 — Windows x64 translation and restricted execution

- [x] Dynarmic JIT executes a compiler-generated iOS ARM64 Mach-O function on Windows x64
- [x] Bounded guest address space and W^X runtime writes
- [x] Synthetic MH_EXECUTE mapping and guarded code execution
- [x] Verify Darwin SVC guest write/exit bridge on Windows x64 in GitHub CI (run 37751343388)
- [x] Verify deterministic bundle dependency planner on all supported platforms (run 37751343388)
- [x] Link owned MH_EXECUTE/MH_DYLIB fixtures and execute one controlled linked function on both Windows x64 and ARM64 (run 37751343388)
- [x] Narrow owned-dylib import and chained fixups staged atomically (run 37751343388)
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
## Parallel host backend development

- [x] Windows x64 executes owned iOS ARM64 MH_OBJECT via Dynarmic
- [x] Windows ARM64 executes an owned ARM64 MH_OBJECT function directly (host memory RX)
- [x] Linux ARM64 executes the same owned ARM64 function directly
- [x] Shared CPU register/event interface with separated Dynarmic and restricted native backend (run 37752367548)
- [ ] Cross-platform bounded run-until-event execution tests on latest commit
- [ ] Real linked iPhoneOS executable/dylib inter-module call on Windows x64
- [ ] Repeat the identical linked dependency execution on Windows ARM64
- [ ] Host-specific Darwin SVC interception with no guest syscall escaping to host kernel
- [ ] First graphical owned .app launches on both Windows variants

## Memory, dyld and native ABI hardening

- [x] Define and test 16 KiB iOS arm64 guest-page policy with 4 KiB private backing (run 37752529646)
- [ ] Confirm page-mapping rollback journal with latest full CI
- [ ] Confirm signed chained import formats 2/3 with latest full CI
- [ ] Confirm explicit SVC refusal in native ARM64 backend with latest CI
- [ ] Test Apple ABI ↔ Windows ARM64 typed thunk for x18, narrow arguments and variadics
- [ ] Enforce a separately isolated native guest process (the current owned linked CI test runs in-process)
- [ ] Link owned iPhoneOS fixtures on Linux/Windows with original libSystem .tbd metadata
