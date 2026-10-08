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

## Scoped process and TLS milestones

- [x] Execute owned SDK-free C `_malloc` → `_write` → `_exit` through Dynarmic Windows x64, verified run 37763147842
- [x] Execute owned linked hello C process under Dynarmic with LC_MAIN, argv/envp/apple, constructor, exact stdout `hello\\n` and exit 23 (run 37766283476)
- [x] Verify modern `__init_offsets` and legacy `__mod_init_func` initializer CTests on Linux GCC/Clang, macOS Clang and Windows MSVC (run 37766283476)
- [x] Verify initial unconfigured MRS/MSR refusal tests on Dynarmic/native (run 37766283476); native refusal remains enforced (run 37770789789)
- [x] Run **owned Clang Darwin TLV** with TPIDRRO_EL0 and __thread_vars host resolver across **two sequentially switched emulated guest threads** under Dynarmic; run 37770789789, step `Execute owned Clang iOS TLV process in two guest threads`
- [ ] Full Darwin TLS: concurrent guest pthread scheduling, destructors/teardown, native ARM64 register isolation, broad ABI validation
- [ ] Windows ARM64 ABI hardware x18/callee-saved register test in separate isolated process

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
- [x] Cross-platform bounded run-until-event execution tests (full CI run 37754360212)
- [x] Real linked iPhoneOS executable/dylib inter-module call on Windows x64 (run 37751343388)
- [x] Repeat the identical linked dependency execution on Windows ARM64 (run 37751343388)
- [ ] Host-specific Darwin SVC interception with no guest syscall escaping to host kernel
- [ ] First graphical owned .app launches on both Windows variants

## Focused ABI, memory and libSystem validations

- [x] Typed Apple scalar variadic stack-slot decoding, 16-byte SP alignment and sign-extension tests (standard CI 37762837584)
- [x] Enforce 16 KiB guest code/data pages; allow only final read-only __LINKEDIT rounded VM padding (Windows stage and CTest run 37762837584)
- [x] One 64 KiB-aligned native Windows ARM64 address reservation; commit/protect per 16 KiB logical guest page and verify uncommitted guard gap (CI 37762837584)
- [x] Scan each trusted native executable mapping for SVC opcodes before RX, rejecting literal-pool false positives (CI 37762837584)
- [x] SECURITY.md restricts in-process native guest execution to trusted owned fixtures
- [x] SDK-free authored libSystem.tbd with exact _malloc/_write/_exit imports and host-only bounded implementation (link/Ctests in run 37762837584)
- [x] Execute all three imported libSystem operations from original owned linked guest C program via Dynarmic on Windows x64 (run 37763147842)
- [ ] Demonstrate real hardware Windows ARM64 x18 preservation across an Apple/Windows ABI thunk in a separate process

## Memory, dyld and native ABI hardening

- [x] Define and test 16 KiB iOS arm64 guest-page policy with 4 KiB private backing (run 37752529646)
- [x] Confirm page-mapping rollback journal with full 11-job CI (run 37753473659)
- [x] Signed chained import formats 2/3 with negative tests verified in full CI (run 37753473659)
- [x] Explicit guest SVC refusal in native Windows ARM64 CI job (run 37753783845)
- [ ] Test Apple ABI ↔ Windows ARM64 typed thunk for x18, narrow arguments and variadics
- [ ] Enforce a separately isolated native guest process (the current owned linked CI test runs in-process)
- [x] Link original iPhoneOS fixtures on Ubuntu Linux with project-authored libSystem .tbd metadata (job sdk-free-ios-link, run 37759173521)
- [ ] Confirm SDK-free LLVM linker fixture on Windows without an Apple SDK
- [x] Fixed-scalar typed guest ABI callback tests (Windows MSVC, Linux and macOS, run 37759173521)
- [ ] Apple ARM64 caller ↔ Windows ARM64 host ABI thunk conformance tests (varargs, aggregates and x18)
