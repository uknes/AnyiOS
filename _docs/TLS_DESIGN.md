# Darwin ARM64 thread-local storage plan

Status: **Narrow owned-fixture TLV implemented and CI verified on Windows x64 Dynarmic** (run 37770789789, `Execute owned Clang iOS TLV process in two guest threads`: https://github.com/uknes/AnyiOS/actions/runs/37770789789). Two sequentially switched **emulated guest threads** preserve independently initialized guest TLS. **Not supported:** real concurrent guest pthread scheduling, TLS destructors/teardown, general Darwin TLS, native ARM64 TLS. Previous native MRS/MSR refusal evidence: run 37766283476.

## Architecture: two host backends, one guest TLS model

The guest's TLS state is per emulated **Darwin thread**, never per arbitrary Windows/Linux host thread. Implemented so far: validated per-thread TPIDRRO_EL0 values, bounded 16 KiB guest TLS pages, own Mach-O `__thread_vars` descriptor validation, module initialization templates/zero-fill and stable guest pointers. Pending: distinct writable TPIDR_EL0 where appropriate, actual guest-thread scheduler, and destructor execution; thread teardown **throws** rather than silently succeeding.

On **Dynarmic/windows-x64**, the permissively licensed `UserConfig.tpidrro_el0` points to an AnyiOS-owned 64-bit register holding a **validated guest virtual address**. `CpuState` carries `guest_thread_id`, `tpidrro_el0`, and `tpidrro_valid`; switching explicitly saved `CpuState` swaps the emulated guest TLS context. `MRS TPIDRRO_EL0` works only with valid context and readable guest memory; all other MRS/MSR remain unsupported. Guest TPIDRRO_EL0 never resolves to host OS TEB/TLS.

For owned Darwin Clang `__thread` programs, `GuestTls` checks S_THREAD_LOCAL_REGULAR, S_THREAD_LOCAL_ZEROFILL and S_THREAD_LOCAL_VARIABLES types, validates descriptors and treats the third descriptor word as a **byte offset into its module's TLV template**. The original Clang-generated `__tlv_bootstrap` import enters an AnyiOS-controlled SVC host resolver: guest descriptor in x0, returned **guest virtual address** in x0. The allocator lazily initializes each module's template separately for each **emulated guest thread**. Unknown keys, invalid mapping, missing thread context and budget exhaustion throw; teardown/destructors are refused, not stubbed. No raw host pointer crosses the bridge.

On **native Windows ARM64**, the host owns its platform register x18, and guest TPIDRRO_EL0 values cannot be assumed interchangeable with host OS TLS. The existing native linked-execution fixture is in-process; it has no secure trap/exception boundary. Until native guest code is in an isolated process with a validated fault/sysregister intercept or a restricted verified rewrite strategy, AnyiOS must not execute real guest MRS/MSR or TLV instructions natively.

## Current fail-closed boundary (narrow Dynarmic TLV only)

- Dynarmic's `CpuBackend::step` examines the actual guest instruction fetched through permission-checked GuestMemory; it permits only `MRS TPIDRRO_EL0` when configured for a validated emulated thread, and refuses other MRS/MSR with `CpuEventKind::unsupported` before guest execution.
- The restricted native fixture CPU backend already accepts only an owned MOVZ/RET pair and additionally labels MRS/MSR as unsupported.
- The trusted Windows ARM64 linked fixture preflight scans all new RX executable image segments and denies any MRS/MSR-shaped 32-bit word, including a *literal-pool false positive*, before host execution. The original SVC scan rules (immutable RX, rescan every new executable mapping, no security guarantees for runtime modification) remain in force.
- Tests: `native-svc-preflight` refuses native MRS/MSR. `arm64-on-x64` checks unconfigured MRS/MSR refusal, configured TPIDRRO guest reads, two switched guest contexts, original Clang TLV fixture execution with per-thread values (12, 12, 17), malformed descriptors, and teardown refusal. Run 37770789789.

## Follow-up acceptance beyond the verified owned TLV slice

1. Extend ABI coverage and strictly bounded descriptors before treating unknown Clang/TLV layouts as supported.
2. Demonstrate a real guest-thread scheduler and deterministic interleaving/concurrency (today: **sequential context switching only**).
3. Add correct TLV teardown/destructors and module unload or preserve explicit fail-closed refusal.
4. Native ARM64 TLS remains blocked by process isolation and explicit system-register interception; matching ISA alone never proves TLS.

Reference (behavior/documentation only): permissively licensed Dynarmic A64 UserConfig API and published Apple ARM64 ABI/TLV descriptions. No GPL/LGPL/APSL source has been copied.