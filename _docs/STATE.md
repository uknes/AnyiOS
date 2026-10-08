# AnyiOS engineering state — evidence-based

Updated 2026-10-08. Source history and green GitHub Actions job results are authoritative.

## Verified focused milestones (CI run 37762837584)

- Apple ARM64 stack-passed scalar varargs are marshaled from guest 8-byte stack slots (not host register varargs); 16-byte SP validation and sign/zero extension are covered by the cross-platform ABI CTest. Other argument classes (pointers, aggregates, unsafe va_list) deliberately fail.
- Windows ARM64 trusted native fixture reserves a 64 KiB-aligned arena once, commits validated guest regions per iOS 16 KiB page, verifies an unmapped guard gap, scans each executable mapping for any SVC-shaped word (including literal-pool false positives), then applies RX permissions. Native **untrusted execution is forbidden**. Windows ARM64 linked-staging and SVC scan tests passed.
- The Linux SDK-free LLVM linker emits project-owned iPhoneOS images that can be staged on both Windows x64 and ARM64 through the 16 KiB guest page loader. The only permitted short-VM mapping exception is the **final read-only __LINKEDIT segment**, rounded up to one or more full 16 KiB guest pages. Code/data segments with 4 KiB VM extents are rejected.
- Original SDK-free C fixture imports `_malloc`, `_write`, `_exit` from a metadata-only libSystem.tbd; the Linux linker verifies those names. Host-side `LibSystemShim` unit tests validate guest heap allocations, captured write, exit signaling and missing-symbol rejection. This is not yet guest CPU execution of the imported functions.

## Under CI review — do not claim compatible execution

- The guest-to-host callback adapter verifies x18/x19–x29 logical guest register preservation, restorative failure behavior and native/translated backend callbacks. Host machine x18 preservation by a genuine native assembly ABI thunk remains unverified.
- The Windows x64 Dynarmic test of the **original SDK-free linked LibSystemApp calling _malloc→_write→_exit** is committed at https://github.com/uknes/AnyiOS/commit/7e4ccee33fa6de0cd3684f71c345577b0200dbb1 and is **not verified** until its workflow execution step is green.

## Verified milestones

- **Genuine iPhoneOS linked MH_EXECUTE → MH_DYLIB call** executed and returned 42 on **both Windows x86-64 (Dynarmic)** and **Windows ARM64 (trusted native CI fixture)**: https://github.com/uknes/AnyiOS/actions/runs/37751343388.
- Independent Apple iPhoneOS toolchain compiles the owned fixture pair; its exact bytes are transferred to both Windows architectures for Mach-O parsing, dylib import binding, staged fixes and CPU execution. CI: https://github.com/uknes/AnyiOS/actions/runs/37751343388.
- Guest Darwin SVC #0x80 write fixture and rejected unknown calls on Windows x64 via Dynarmic: https://github.com/uknes/AnyiOS/actions/runs/37751343388.
- **Dynarmic moved out of tests** into `src/dynarmic_backend.cpp`; `src/native_backend.cpp` shares the typed CPU register/event contract. The native path is still restricted to an owned two-instruction fixture. CI: https://github.com/uknes/AnyiOS/actions/runs/37752367548.
- 16 KiB iOS guest-page API with private 4 KiB backing granules and alignment tests: https://github.com/uknes/AnyiOS/actions/runs/37752529646.
- Restricted chained pointer formats 2/6 and import format 1, symbol/export trie lookup, fixup staging and imported-call guest memory tests: https://github.com/uknes/AnyiOS/actions/runs/37751343388.

## Recently verified hardening

- A page-level `GuestMemory::MappingJournal` replaces `auto draft = memory` and supports whole-library-pair rollback. Its memory, synthetic loader, staged-image, native and x64 integration tests passed the complete 11-job run: https://github.com/uknes/AnyiOS/actions/runs/37753473659.
- Signed chained import **format 2 (32-bit addend)** and **format 3 (64-bit addend)**, with bounds checking and signed overflow/underflow negative tests, passed the same complete run.
- The Windows ARM64 native backend rejected guest Darwin SVC before attempting host execution, verified by successful native ARM64 job: https://github.com/uknes/AnyiOS/actions/runs/37753783845.

## Additional verified contracts

- Bounded `CpuBackend::run_until_event` and import64 reserved-bit validation passed the full CI matrix: https://github.com/uknes/AnyiOS/actions/runs/37754360212.
- **Fixed integer-only ABI thunk** converts signed/unsigned 8/16/32/64-bit guest register arguments into explicitly registered host callbacks; tests verify signed extension, x18/context preservation, and fail-closed variadic, pointer, aggregate and missing-host-function cases. The Windows MSVC, Linux GCC/Clang and macOS compiler jobs passed: https://github.com/uknes/AnyiOS/actions/runs/37759173521. This is not an arbitrary libSystem call bridge.
- **SDK-free iPhoneOS linker fixture** passed the dedicated Ubuntu 24.04 job: LLVM clang-19/ld64.lld-19 generated the original ARM64 MH_EXECUTE/MH_DYLIB using a metadata-only authored libSystem.tbd. The produced images passed the AnyiOS inspector. Same evidence: https://github.com/uknes/AnyiOS/actions/runs/37759173521. Linking proves no runtime API implementations.
- Open-source license survey for 23 candidates, actual license-file paths, commit SHAs and verdicts: [_docs/OSS_SURVEY.md](OSS_SURVEY.md).

## Critical missing features

- Fully general dyld dependency and dynamic module initialization; legacy fixups, ARM64e PAC, shared cache, weak/reexport behavior.
- Safe execution of arbitrary native guest code: process sandbox, Darwin SVC interception, exception/guard-page handling and ABI thunks.
- Apple-compatible libSystem, Mach IPC, threading, Objective-C/Swift runtime, Foundation/CoreFoundation/UIKit, graphics/audio/input and a proper `.app` lifecycle.
- Modern Metal/AIR graphics translation, application entitlement compatibility and protected IPA handling.

## Next engineering actions

1. Preserve the now-verified journal, signed import formats and bounded run-until-event regression suites across new loader changes.
2. Extend the verified fixed-scalar ABI marshaler with a separately isolated native ARM64 guest process and a real Apple-compiled caller fixture; variadics and guest pointers remain forbidden.
3. Stage two-module tests that call an owned libSystem-compatible symbol; no dummy successful stubs.
4. Compare the verified SDK-free iOS ARM64 LLVM link products to the Apple-linked oracle and then test SDK-free linking on Windows.
5. Only then work on Objective-C runtime and a first actual owned app lifecycle.

Research / decision docs: [OSS survey](OSS_SURVEY.md), [ABI risks](ABI_BRIDGE.md), [architecture decisions](DECISIONS.md), [linked execution proofs](EXECUTION_PROOFS.md).
