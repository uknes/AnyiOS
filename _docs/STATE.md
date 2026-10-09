## 2026-10-09 — multi-image constructor prerequisite (owned scope CI verified)

Dependency-first owned initializer planning, stricter initializer descriptor/code ownership, and a real three-image Clang ARM64 constructor chain are implemented. Local GCC host contracts and exact implementation head `827ce8e58d5c060d8ed2d7e4d7ed5f28c01199ce` main CI passed: [run 37974381605](https://github.com/uknes/AnyiOS/actions/runs/37974381605), 14/14 jobs. Windows x64 Dynarmic executed all three original owned constructors in leaf/middle/main order and LC_MAIN returned 735. Native Windows/Linux ARM64 coverage is host planning only. The C/C++ constructor gate is now partial (12 partial gates total); verified count remains 6/575. Original third-party app startup is unchanged. See [contract and acceptance](MULTI_IMAGE_INITIALIZERS.md).

## First external Objective-C iOS app compatibility checkpoint (verified on PR head)

- Pinned external target: Bitrise `sample-apps-ios-simple-objc` commit `91fef6f5a096220669934793a9256128bc73f25b`; actual upstream LICENSE is MIT. Original `main.m`, `AppDelegate.m`, `ViewController.m` compiled without Apple SDK using **project-authored declaration-only headers**; link uses **metadata-only unresolved symbol TAPI** (not UIKit/Foundation/CoreData/ObjC implementations).
- **Static gap:** 29 distinct imported symbols, 9 imported external ObjC classes, 40 method-name candidates, 24 unresolved encoded selector pointers. No imported symbol from this app is satisfied by the verified guest shim subset. Analyzer uses static occurrence evidence, not runtime call frequencies.
- **Windows x64 static stage passed:** loader mapped external arm64 Mach-O sections and chained fixups, entry `81920`, with 29 deliberately unresolved placeholders. CI run **37798177618**, job `ios-gap-windows-x64`, step `Stage original Objective-C app Mach-O on Windows x64; no guest execution`.
- **Windows x64 Dynarmic first actual external app entry execution passed:** bounded instruction execution reached and **stopped at `_objc_autoreleasePoolPush`**. Constructor/ObjC class registration intentionally not executed. CI run **37798177618**, job `ios-entry-windows-x64`, step `Execute real app entry until first missing iOS runtime import (fail closed)`; earlier verified run **37798025355** passed same step. This is **not an app launch**.
- Main CI and compatibility workflows green on PR #4 exact head `bcd3dd32bd29a81999b273becff28176939a2b2b`: runs **37798177768** (14/14 jobs) and **37798177618** (3/3 jobs). Merged into main as `860218415c6b03898deb9acd984e06ff27170cf0`. Main-branch post-merge run verification tracked separately.
- **Not implemented:** ObjC autorelease pools/messaging/class registration, Foundation, UIKit, Core Data, UI/window/touch, full app initializer order; five-app cross-application ranking. PR #3 memory-string, overlap-safe chunked copy, errno/abort/stdio/pthread, ObjC ABI notes and hot-path ADR still pending.

## TLS resource and teardown limits (owned-fixture subset)

- Guest thread exit and TLS destructor/teardown are **unsupported**; `finish_thread` refuses rather than reporting success.
- TLV allocation is **one 16 KiB page per (guest thread, module) pair**, drawn from a **1 MiB arena**; this is not general dynamic Darwin TLS allocation.
- The guest thread header is mapped **read/write (RW)**; do not describe it as immutable or read-only.
- The TLV descriptor's reserved word **must be zero**. Nonzero reserved fields are rejected.
- These restrictions apply even though owned Clang two-thread TLV execution passed Windows x64 Dynarmic in CI run **37770789789**, step `Execute owned Clang iOS TLV process in two guest threads`. No concurrent host-thread or native Windows ARM64 TLS support is claimed.

# AnyiOS evidence checkpoint

Updated 2026-10-08. Committed source + successful GitHub Actions job evidence is authoritative.

## Verified

- **Windows x64 Dynarmic `_malloc` / `_write` / `_exit` guest execution** using the original SDK-free ARM64 C binary and registered SVC thunks. The captured stdout was `OK` and guest exit code was zero. Run **37763147842**, job `windows-a64-translation`: https://github.com/uknes/AnyiOS/actions/runs/37763147842.
- The independent Apple-linked owned iPhoneOS executable and dylib execute a cross-dylib return-42 call on Windows x64 and ARM64 (the latter in trusted in-process CI only): run 37751343388.
- Windows x64/ARM64 stage original SDK-free LLVM-linked ARM64 Mach-O; restricted 16 KiB guest pages, final read-only LINKEDIT rounding, transactional guest memory journal: run 37762837584.
- ABI fixed-scalar/stack-variadic unit tests, guest host-to-guest callbacks, native SVC scan, bounded host libSystem contract: run 37762837584 and 37763147842.
- Linux/macOS/Windows Mach-O parser regression/fuzzer, negative dyld fixup formats and pointer validations: prior green CI runs listed in ROADMAP.

## Newly CI verified (run 37770789789)

- **Windows x64 Dynarmic controlled Darwin TLV fixture:** project-owned Clang `arm64-apple-ios` `TlvProcess` linked without Apple SDK, generated real `__thread_vars`/`__thread_data`/`__thread_bss`, and executed the original `__tlv_bootstrap` import through an AnyiOS host ABI gate. Two **sequentially switched emulated Darwin thread contexts** preserved independent initialized counters (12, 12, 17). Dynarmic reads a validated per-guest-thread `TPIDRRO_EL0` pointer from guest memory; arbitrary MRS/MSR still fail closed. Run **37770789789**, job `windows-a64-translation`, exact step `Execute owned Clang iOS TLV process in two guest threads`: https://github.com/uknes/AnyiOS/actions/runs/37770789789. Portable `guest-darwin-tlv-contract` passed Linux GCC/Clang, Windows MSVC, macOS Clang in the same run.
- **Narrow scope:** this is original owned-fixture guest TLV support, *not* full Darwin TLS or actual concurrent pthread execution. Guest thread teardown/destructors explicitly throw, and native Windows ARM64 remains MRS/MSR fail-closed. No host pointer is returned to the guest.

## Previously CI verified (run 37766283476)

- **Complete project-owned `HelloProcess` LC_MAIN execution under Dynarmic on Windows x64:** SDK-free arm64-apple-ios C fixture was linked with authored libSystem metadata; one real `__TEXT,__init_offsets` constructor executed before main, followed by argc/argv/envp/apple vector validation, guest `_malloc`, `_write` output `hello\\n`, and `_exit(23)`. The exact `Boot owned iOS hello-world process` step passed. https://github.com/uknes/AnyiOS/actions/runs/37766283476.
- **Initializer format tests** for modern `__init_offsets` and legacy `__mod_init_func`, strict 16 KiB guest stack, zero-terminated vectors and malformed stack refusal all passed on Linux GCC/Clang, Windows MSVC and macOS Clang.
- **Original TLS fail-closed tests** verified native Windows ARM64/Linux ARM64 MRS/MSR rejection and `native-svc-preflight` detection; with run 37770789789 Dynarmic now permits only configured guest `MRS TPIDRRO_EL0` and rejects other unsupported system registers. Native remains unchanged.

## Implemented and previously verified (scope limited to owned fixtures)

- Original LLVM-linked **HelloProcess** C binary checks `argc/argv/envp/apple`, constructor state, calls `malloc`, writes `hello\\n` via `write`, and exits 23.
- `prepare_owned_process_stack` constructs a 16-byte-aligned bounded guest stack with null-terminated argv/envp/apple vectors and maps it using 16 KiB iOS guest pages.
- `find_owned_module_initializers` accepts legacy `__mod_init_func` pointer tables and modern linker-generated `__TEXT,__init_offsets` tables; target pointers must be executable and inside mapped guest memory. A focused CTest covers both.
- The production-style Darwin ABI, threads and process isolation are still absent; this is an **owned deterministic process fixture**, not general IPA launch.
- Future CI work on multi-module initializer ordering, full native/concurrent thread TLS and ObjC runtime remains pending.

## Not implemented

- A safe, isolated Windows ARM64 guest process capable of arbitrary Darwin SVC, system register or thread-local operations.
- General dyld graph, complete C runtime, real concurrent pthreads, guest TLV destructors/thread teardown, native ARM64 TLV, writable TPIDR_EL0 emulation, Mach IPC, ObjC/Swift, Foundation/UIKit, app window, graphics, commercial IPAs.
- Real hardware x18 and callee-saved register preservation across a native Apple↔Windows ABI thunk.

## Decision

ADR-012 recommends **host-side libSystem shims with explicit ABI contracts first**; guest-side musl (MIT) or FreeBSD libc (file-specific BSD and other terms) is deferred. The actual license files at pinned commits and rationale are recorded in `_docs/DECISIONS.md`. TLS plan: `_docs/TLS_DESIGN.md`.

## Next

1. Expand host-side **libSystem** narrowly using owned ABI fixtures: `memcpy`, `memset`, `strlen`, `strcmp`, `errno`, `abort`, write-backed stdio and single-thread `pthread_once`/mutex; test narrow-int extension and variadic boundaries.
2. Dedicated multi-module initializer ordering regression with dependency order and malformed fixtures.
3. Owned Clang Objective-C section metadata parsing tests and Apple ObjC ABI-vs-libobjc2 ADR (no runtime implementation).
4. ADR performance decision gate on guest-side hot paths versus SVC host shims; revisit ADR-012, **do not implement yet**.
5. Only afterward, isolated Windows ARM64 x18/callee-saved thunk proof.
