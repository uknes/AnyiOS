# AnyiOS evidence checkpoint

Updated 2026-10-08. Committed source + successful GitHub Actions job evidence is authoritative.

## Verified

- **Windows x64 Dynarmic `_malloc` / `_write` / `_exit` guest execution** using the original SDK-free ARM64 C binary and registered SVC thunks. The captured stdout was `OK` and guest exit code was zero. Run **37763147842**, job `windows-a64-translation`: https://github.com/uknes/AnyiOS/actions/runs/37763147842.
- The independent Apple-linked owned iPhoneOS executable and dylib execute a cross-dylib return-42 call on Windows x64 and ARM64 (the latter in trusted in-process CI only): run 37751343388.
- Windows x64/ARM64 stage original SDK-free LLVM-linked ARM64 Mach-O; restricted 16 KiB guest pages, final read-only LINKEDIT rounding, transactional guest memory journal: run 37762837584.
- ABI fixed-scalar/stack-variadic unit tests, guest host-to-guest callbacks, native SVC scan, bounded host libSystem contract: run 37762837584 and 37763147842.
- Linux/macOS/Windows Mach-O parser regression/fuzzer, negative dyld fixup formats and pointer validations: prior green CI runs listed in ROADMAP.

## Newly CI verified (run 37766283476)

- **Complete project-owned `HelloProcess` LC_MAIN execution under Dynarmic on Windows x64:** SDK-free arm64-apple-ios C fixture was linked with authored libSystem metadata; one real `__TEXT,__init_offsets` constructor executed before main, followed by argc/argv/envp/apple vector validation, guest `_malloc`, `_write` output `hello\\n`, and `_exit(23)`. The exact `Boot owned iOS hello-world process` step passed. https://github.com/uknes/AnyiOS/actions/runs/37766283476.
- **Initializer format tests** for modern `__init_offsets` and legacy `__mod_init_func`, strict 16 KiB guest stack, zero-terminated vectors and malformed stack refusal all passed on Linux GCC/Clang, Windows MSVC and macOS Clang.
- **Current TLS fail-closed policy** is verified, not TLS functionality: the Windows x64 Dynarmic smoke test denies guest MRS/MSR, Windows ARM64/Linux ARM64 native fixture tests reject guest MRS/MSR, and the cross-platform `native-svc-preflight` CTest rejects instruction/data lookalikes. No TPIDRRO_EL0 or _tlv_get_addr implementation exists.

## Implemented — CI verification pending

- Original LLVM-linked **HelloProcess** C binary checks `argc/argv/envp/apple`, constructor state, calls `malloc`, writes `hello\\n` via `write`, and exits 23.
- `prepare_owned_process_stack` constructs a 16-byte-aligned bounded guest stack with null-terminated argv/envp/apple vectors and maps it using 16 KiB iOS guest pages.
- `find_owned_module_initializers` accepts legacy `__mod_init_func` pointer tables and modern linker-generated `__TEXT,__init_offsets` tables; target pointers must be executable and inside mapped guest memory. A focused CTest covers both.
- The production-style Darwin ABI, threads and process isolation are still absent; this is an **owned deterministic process fixture**, not general IPA launch.
- Future CI work on multi-module initializer ordering, _tlv_get_addr and ObjC runtime remains pending.

## Not implemented

- A safe, isolated Windows ARM64 guest process capable of arbitrary Darwin SVC, system register or thread-local operations.
- General dyld graph, C runtime initialization, TLV/TPIDRRO state, Mach IPC, ObjC/Swift, Foundation/UIKit, app window, graphics, commercial IPAs.
- Real hardware x18 and callee-saved register preservation across a native Apple↔Windows ABI thunk.

## Decision

ADR-012 recommends **host-side libSystem shims with explicit ABI contracts first**; guest-side musl (MIT) or FreeBSD libc (file-specific BSD and other terms) is deferred. The actual license files at pinned commits and rationale are recorded in `_docs/DECISIONS.md`. TLS plan: `_docs/TLS_DESIGN.md`.

## Next

1. Preserve green owned Windows x64 LC_MAIN hello-world process execution and expand initializer ordering only with dedicated regression tests.
2. Verify MRS/MSR negative tests on Dynarmic and Windows/Linux ARM64 native, and add first owned Clang TLV fixture.
3. Investigate isolated Windows ARM64 native host ABI thunk (x18/stack/callee-saved) only after 1–2.
