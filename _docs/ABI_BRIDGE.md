# Apple ARM64 guest ↔ Windows ARM64 host ABI boundary

Audit date: 2026-10-08. This is a **design and conformance plan**, not a claim that libSystem or an arbitrary Apple ARM64 binary can call native Windows functions.

## Host-independent guest execution contract

`include/anyios/cpu_backend.hpp` defines a bounded register snapshot (x0-x30, PC, SP, PSTATE), step / run-until-event, and explicit SVC/fault/unsupported/returned outcomes. The Dynarmic adapter now lives at `src/dynarmic_backend.cpp`. The ARM64 native adapter is `src/native_backend.cpp`, but it **only allows a whitelisted two-instruction owned fixture**; it rejects SVC and unknown instructions without executing them. A separate trusted linked-image Windows ARM64 CI runner bypasses that adapter and directly executes the authored application/dylib within an in-process arena; it must **not** be marketed as safe general native guest execution.

## Required ABI distinctions

1. **x18:** Apple reserves x18 for its platform. Windows ARM64 uses x18 as a host thread-environment pointer (TEB). A thunk or signal/trap layer must preserve its host meaning and prevent guest register state from corrupting it. Guest and host register contexts need explicit save/restore. The present owned fixture never touches x18.
2. **Small integer arguments:** Apple requires the *caller* to sign- or zero-extend integer arguments narrower than 32 bits. The standard/Windows AAPCS64 convention may place that responsibility differently. Thunks need signature-aware extension and tests for signed char, unsigned char, bool and 16-bit values.
3. **Variadic functions:** Apple ARM64 packs its variadic arguments using Apple-specific stack rules and uses a different `va_list` representation; Windows ARM64 cannot reinterpret guest va_list or reuse C varargs forwarding. A typed argument descriptor/packer must handle fixed and variadic arguments separately; unknown signatures fail.
4. **Alignment and aggregates:** Apple supports some 16-byte-aligned arguments in odd-numbered xN registers and can pack stack arguments in smaller slots than generic AAPCS64. A struct/vector/long double/indirect result bridge must follow the original function signature, not default to a host C cast.
5. **SVC #0x80 and Darwin x16:** A native guest syscall must never fall through into the Windows host kernel. The restricted native backend tests reject the instruction before execution. The CI-only in-process linked runner has **no arbitrary-code trap boundary** and is restricted to the owned fixture only.
6. **Guard pages and virtual memory:** iOS ARM64 has 16 KiB guest pages. `GuestMemory` uses 4 KiB private backing granules internally but `map_ios` enforces the logical 16 KiB policy for actual linked-image staging. Protected/guard pages must reject guest fetch, read and write and preserve host process isolation.
7. **Callbacks and exceptions:** Objective-C message sends, Swift trampolines, block invoke pointers, TLS, stack unwinding, PAC and native host↔guest callbacks all require explicit crossing points. No raw guest function pointers or pointers-to-host memory are passed to host APIs.

## Existing regression evidence

- Windows x64 Dynarmic guest SVC and linked cross-dylib return 42: https://github.com/uknes/AnyiOS/actions/runs/37751343388.
- Shared CPU backend contract extraction and its original CTest clients: https://github.com/uknes/AnyiOS/actions/runs/37752367548.
- Native Windows ARM64, Linux ARM64: in-process own code, CPU contract tests and guest guard-page validation: https://github.com/uknes/AnyiOS/actions/runs/37752367548.
- Explicit native SVC rejection test committed in `tests/native_a64_smoke.cpp`; verify its latest Windows ARM64 job before recording it as passed.
- Owned Windows ARM64 app→dylib return 42 in trusted CI sandbox **runner** (not a sandboxed guest): https://github.com/uknes/AnyiOS/actions/runs/37751343388.

## Required tests before the first libSystem host thunk

- A cross-compiled Apple AArch64 caller and Windows ARM64 host callee for signed/unsigned narrow arguments and mixed structs.
- Apple-ABI variadic stack fixture and typed `va_list` bridge (a host function with `...` must not receive raw Apple va_list).
- Host x18 nonclobber and restoration tests in a separately isolated subprocess.
- At least one guest callback hosted from a Win32 stub with safe guest stack/state restoration.
- SVC x16 dispatch without letting guest instruction execute a Windows SVC natively.
- 16 KiB virtual guest stack guard, stack alignment, 2+ modules and transactional failed mapping, with ASAN/CI validation.
- Process-level isolation for native guest execution; no arbitrary IPA support before it exists.

References (documentation only, no Apple implementation copied):
- https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
- https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions
- https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/AboutMemory.html

## Verified fixed-scalar host callback contract

`src/abi_thunk.cpp` and `include/anyios/abi_thunk.hpp` implement a narrowly bounded host callback ABI. Calls accept a **declared non-variadic signature of at most eight integer-register arguments**. The bridge extracts low 8/16/32-bit signed and unsigned values, marshals them to host 64-bit scalar values, and invokes only an explicitly registered callback. Pointer values, aggregates, variadics, ninth arguments and missing callbacks throw `std::invalid_argument` instead of entering the host ABI. The caller's `CpuState` (including x18, PC and SP) remains unchanged.

Evidence: Windows MSVC, Linux GCC/Clang and macOS jobs in [run 37759173521](https://github.com/uknes/AnyiOS/actions/runs/37759173521).

**Limitations:** This helper is **not** a Windows machine-code Apple↔Windows ABI assembly thunk. It cannot pass host pointers, implement Apple varargs/va_list, handle arm64e PAC, or guarantee native guest x18 preservation across a foreign call. All those capabilities remain blocked until dedicated call-gate and isolation tests exist.
