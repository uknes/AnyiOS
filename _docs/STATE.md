# AnyiOS evidence checkpoint

Updated 2026-10-08. Committed source + successful GitHub Actions job evidence is authoritative.

## Verified

- **Windows x64 Dynarmic `_malloc` / `_write` / `_exit` guest execution** using the original SDK-free ARM64 C binary and registered SVC thunks. The captured stdout was `OK` and guest exit code was zero. Run **37763147842**, job `windows-a64-translation`: https://github.com/uknes/AnyiOS/actions/runs/37763147842.
- The independent Apple-linked owned iPhoneOS executable and dylib execute a cross-dylib return-42 call on Windows x64 and ARM64 (the latter in trusted in-process CI only): run 37751343388.
- Windows x64/ARM64 stage original SDK-free LLVM-linked ARM64 Mach-O; restricted 16 KiB guest pages, final read-only LINKEDIT rounding, transactional guest memory journal: run 37762837584.
- ABI fixed-scalar/stack-variadic unit tests, guest host-to-guest callbacks, native SVC scan, bounded host libSystem contract: run 37762837584 and 37763147842.
- Linux/macOS/Windows Mach-O parser regression/fuzzer, negative dyld fixup formats and pointer validations: prior green CI runs listed in ROADMAP.

## Implemented — CI verification pending

- Original LLVM-linked **HelloProcess** C binary checks `argc/argv/envp/apple`, constructor state, calls `malloc`, writes `hello\\n` via `write`, and exits 23.
- `prepare_owned_process_stack` constructs a 16-byte-aligned bounded guest stack with null-terminated argv/envp/apple vectors and maps it using 16 KiB iOS guest pages.
- `find_owned_module_initializers` accepts legacy `__mod_init_func` pointer tables and modern linker-generated `__TEXT,__init_offsets` tables; target pointers must be executable and inside mapped guest memory. A focused CTest covers both.
- Windows x64 Dynarmic `--hello` CI step runs initializer before LC_MAIN, then verifies exact captured output and exit.
- Darwin guest **TLS still unavailable**; both MRS/MSR instruction classes now reject via Dynarmic CPU step/native backend and native executable mapping scanner, with original negative tests. This must pass CI before being called verified.
- Latest CI for initializer support: https://github.com/uknes/AnyiOS/actions/runs/37766283476

## Not implemented

- A safe, isolated Windows ARM64 guest process capable of arbitrary Darwin SVC, system register or thread-local operations.
- General dyld graph, C runtime initialization, TLV/TPIDRRO state, Mach IPC, ObjC/Swift, Foundation/UIKit, app window, graphics, commercial IPAs.
- Real hardware x18 and callee-saved register preservation across a native Apple↔Windows ABI thunk.

## Decision

ADR-012 recommends **host-side libSystem shims with explicit ABI contracts first**; guest-side musl (MIT) or FreeBSD libc (file-specific BSD and other terms) is deferred. The actual license files at pinned commits and rationale are recorded in `_docs/DECISIONS.md`. TLS plan: `_docs/TLS_DESIGN.md`.

## Next

1. Finish green Windows x64 `--hello` process step; fix actual CI errors before checking its ROADMAP milestone.
2. Verify MRS/MSR negative tests on Dynarmic and Windows/Linux ARM64 native, and add first owned Clang TLV fixture.
3. Investigate isolated Windows ARM64 native host ABI thunk (x18/stack/callee-saved) only after 1–2.
