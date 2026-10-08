# AnyiOS engineering state

Updated: 2026-10-08. Source commits and successful GitHub Actions jobs are authoritative.

## Current target hosts

- Windows x86-64: Dynarmic ARM64-to-x64 JIT executes original owned Mach-O MH_OBJECT functions and bounded Darwin SVC fixtures. See successful runs in the project's CI history.
- Windows ARM64: optional native executable-page fixture runs project-owned, Clang-compiled ARM64 Mach-O MH_OBJECT function without instruction translation. Verified in https://github.com/uknes/AnyiOS/actions/runs/37748791787.
- Linux ARM64: same native fixture, with mmap/mprotect/cache synchronization and own iOS-target Mach-O function. Verified in https://github.com/uknes/AnyiOS/actions/runs/37748791787.
- Linux/macOS/Windows base: portable Mach-O parser, dependency planner, restricted linker/memory modules and regression suite.

## Implemented analysis and staging

- Thin and universal ARM64 Mach-O inspection, segments, sections, dyld dependencies, symbols and export trie metadata.
- Limited generic-64 chained fixup planning and exact two-level import resolution from explicitly supplied dylib metadata.
- Bounded guest memory permissions, a restricted synthetic MH_EXECUTE loader and a staged linked-image mapper with rollback tests.
- Genuine iPhoneOS MH_EXECUTE and MH_DYLIB are linked on macOS CI, shipped as owned workflow artifacts and inspected on Windows; ordinary linked binaries require libSystem.
- Native ARM64 execution has an exact two-instruction whitelist: it is a safe ABI/CPU proof, NOT arbitrary iOS Mach-O loading.

## Not implemented

- Real multi-module executable execution of the linked iPhoneOS fixture (including libSystem import binding and actual dyld initializers).
- Process isolation, thread semantics, guest stack/exception context, full Darwin syscall ABI/Mach IPC, ObjC/Swift, Foundation/UIKit, graphics/audio/input.
- arm64e authenticated pointers, broader chained pointer variants, Apple dyld shared cache, retail/protected app support.
- An ordinary iOS .app GUI or commercial iOS application on any non-Apple host.

## Next engineering goals

1. Make the staged linked-image mapper accept realistic independently linked iOS library layouts and validate actual fixup metadata.
2. Implement a unified guest CPU runtime interface with distinct Dynarmic/x64 and native ARM64 execution backends; keep guest OS calls isolated.
3. Demonstrate a real cross-dylib call from the project-owned linked iOS executable on Windows x64 and Windows ARM64.
4. Expand explicit libSystem/Darwin contracts with reproducible error behavior.
5. Start ObjC and app window research only after multi-module linking passes.

References: [Windows x64 design](WINDOWS_X64.md), [ARM64 host design](ARM64_HOSTS.md), [dyld subset](DYLD_FIXUPS.md), [research](RESEARCH.md).

No statement about iOS app usability is justified by these CPU smoke tests.
