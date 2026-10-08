# AnyiOS engineering state — evidence-based

Updated 2026-10-08. Source history and green GitHub Actions job results are authoritative.

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

## Still under CI review

- Bounded `CpuBackend::run_until_event` and an additional import64 reserved-bit validation regression are committed, but their newest full run must finish before they are marked as verified.
- Open-source license survey for 23 candidates, with license-path links, commit SHAs and verdicts: [_docs/OSS_SURVEY.md](OSS_SURVEY.md).

## Critical missing features

- Fully general dyld dependency and dynamic module initialization; legacy fixups, ARM64e PAC, shared cache, weak/reexport behavior.
- Safe execution of arbitrary native guest code: process sandbox, Darwin SVC interception, exception/guard-page handling and ABI thunks.
- Apple-compatible libSystem, Mach IPC, threading, Objective-C/Swift runtime, Foundation/CoreFoundation/UIKit, graphics/audio/input and a proper `.app` lifecycle.
- Modern Metal/AIR graphics translation, application entitlement compatibility and protected IPA handling.

## Next engineering actions

1. Confirm green CI for journal, signed import formats and run-until-event; update roadmap only when jobs pass.
2. Add ABI contract fixtures (Apple↔Windows ARM64 varargs, narrow argument extension, x18 preservation, native SVC fail-closed) and process isolation.
3. Stage two-module tests that call an owned libSystem-compatible symbol; no dummy successful stubs.
4. Prototype cross-platform iOS ARM64 linking using LLVM ld64.lld and an original stub-only `libSystem.tbd`, with actual CI evidence.
5. Only then work on Objective-C runtime and a first actual owned app lifecycle.

Research / decision docs: [OSS survey](OSS_SURVEY.md), [ABI risks](ABI_BRIDGE.md), [architecture decisions](DECISIONS.md), [linked execution proofs](EXECUTION_PROOFS.md).
