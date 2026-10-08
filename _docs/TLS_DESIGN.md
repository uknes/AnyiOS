# Darwin ARM64 thread-local storage plan

Status: **TLS remains unimplemented**. MRS/MSR refusal negative tests were verified on 2026-10-08 by Windows x64 Dynarmic, Windows/Linux ARM64 native jobs, and portable SVC scan CTests: https://github.com/uknes/AnyiOS/actions/runs/37766283476.

## Architecture: two host backends, one guest TLS model

The guest's TLS state is per emulated **Darwin thread**, never per arbitrary Windows/Linux host thread. It will own: an architecturally visible TPIDRRO_EL0 guest value; a distinct writable TPIDR_EL0 register where appropriate; a bounded per-thread storage area in guest memory, 16 KiB guest page mappings; per-module TLV keys and initialization templates; and a destructor registry executed at guest-thread termination.

On **Dynarmic/windows-x64**, its permissively licensed UserConfig exposes TPIDRRO_EL0 as a pointer to a host-owned 64-bit guest register. The current AnyiOS configuration leaves this pointer **null**, and does not initialize a valid Darwin TLS base. Accordingly AnyiOS now explicitly refuses all guest ARM64 MRS/MSR instructions at its CPU step boundary rather than allowing an unset TLS value (e.g., 0) to appear successful. A later implementation will supply stable pointers to each guest thread's TLS registers and switch them with saved CPU context; TPIDRRO_EL0 must never resolve to a host TEB/TLS pointer.

For Darwin Clang's thread-local variable ABI, inspect Mach-O `__thread_vars` descriptor sections, TLV init templates and zero-fill regions, then implement the guest `_tlv_get_addr` call convention. It should accept a **validated guest descriptor pointer**, resolve module+offset+thread identity, lazily create storage from the original template, and return a **guest virtual address**. No raw host pointer crosses the bridge. Expose diagnostics for unknown keys, relocations, reentrancy, allocation overflow, missing thread context, or destructor requirements.

On **native Windows ARM64**, the host owns its platform register x18, and guest TPIDRRO_EL0 values cannot be assumed interchangeable with host OS TLS. The existing native linked-execution fixture is in-process; it has no secure trap/exception boundary. Until native guest code is in an isolated process with a validated fault/sysregister intercept or a restricted verified rewrite strategy, AnyiOS must not execute real guest MRS/MSR or TLV instructions natively.

## Current fail-closed behavior (no TLS functionality)

- Dynarmic's `CpuBackend::step` examines the actual guest instruction fetched through permission-checked GuestMemory; it returns `CpuEventKind::unsupported` for MRS or MSR before asking Dynarmic to execute it.
- The restricted native fixture CPU backend already accepts only an owned MOVZ/RET pair and additionally labels MRS/MSR as unsupported.
- The trusted Windows ARM64 linked fixture preflight scans all new RX executable image segments and denies any MRS/MSR-shaped 32-bit word, including a *literal-pool false positive*, before host execution. The original SVC scan rules (immutable RX, rescan every new executable mapping, no security guarantees for runtime modification) remain in force.
- Tests: `native-svc-preflight` rejects both MRS and MSR bytes; `arm64-on-x64` independently injects each opcode and checks unsupported with PC unchanged. No successful TLS accesses or test-stubs are claimed.

## Acceptance for initial TLS implementation

1. Compiler-produced, project-owned iOS ARM64 `_tlv_get_addr` fixture and real Mach-O `__thread_vars` metadata parser with negative fixtures.
2. Dynarmic per-thread register context (TPIDRRO_EL0) and module descriptor state with thread-switch tests; real guest reads and writes use GuestMemory only.
3. Guest _tlv_get_addr returns stable per-thread guest addresses; two threads get independent values, initializer templates are copied, and destructors run or explicitly fail.
4. Native ARM64 work remains blocked by process isolation + explicit MRS/MSR interception; no native TLS "working" claims from matching ISA alone.

Reference (behavior/documentation only): permissively licensed Dynarmic A64 UserConfig API and published Apple ARM64 ABI/TLV descriptions. No GPL/LGPL/APSL source has been copied.