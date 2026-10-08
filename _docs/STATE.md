# Project state — Windows x86-64 target

Updated: 2026-10-08. Source and CI are authoritative. An accepted binary is not necessarily a runnable iOS application.

## Fully verified before this update

- Portable ARM64 Mach-O inspector, universal slices, sections, symbol tables and metadata diagnostics.
- Bounded guest memory with read/write/execute protections, negative fixtures and sanitizer fuzzing.
- Windows x86-64 translation of project-owned, independently compiled ARM64 iOS Mach-O MH_OBJECT function through Dynarmic; return value 42.
- Restricted synthetic MH_EXECUTE mapping and Windows ARM64 execution smoke using page-protected guest memory.
- CI evidence for the compiled object: https://github.com/uknes/AnyiOS/actions/runs/37710397827
- CI evidence for hardened static loader: https://github.com/uknes/AnyiOS/actions/runs/37713669019

## Newly implemented, awaiting full CI verification

- Darwin SVC #0x80 guest trap boundary: limited write(4) to captured stdout/stderr, exit(1) signaling, guest address validation, unsupported-call diagnostics.
- Compiler-produced iOS ARM64 Mach-O assembly fixture exercises guest SVC on a real Windows x64 Dynarmic runner.
- Dependency manifest planner with owned/bundle-relative module registry, @loader_path, @executable_path and @rpath resolution, strong/weak dependency diagnostics and cycle checks.
- Mach-O LC_ID_DYLIB and typed library dependency metadata (weak, reexport, upward).
- CI run for syscall bridge commit: https://github.com/uknes/AnyiOS/actions/runs/37714205260
- The latest dependency-planner commit's CI must pass before those features are recorded as verified.

## Missing before an actual iOS .app can run

- Real linked ARM64 MH_EXECUTE / MH_DYLIB loading, ASLR, dyld chained fixup application, imports, inter-module relocations and module initialization.
- Guest Darwin libSystem API surface, process/thread emulation, complete syscall semantics, Mach ports and IPC.
- Objective-C and Swift runtime ABI support, Foundation/CoreFoundation, UIKit/CoreAnimation, graphics/audio/input and app lifecycle.
- IPA/app bundle installation, Info.plist processing, dynamic system framework substitution and compatibility matrix.
- Reliable arm64e PAC and Metal handling; neither is implemented.

## Next execution milestone

1. Stabilize both the Windows syscall CI and cross-platform dyld planner CI.
2. Build and link a project-owned no-framework MH_EXECUTE and MH_DYLIB fixture, not a hand-synthesized executable.
3. Implement strict dyld fixup and symbol resolution for one owned guest module, with negative tests.
4. Demonstrate owned guest code calling a validated libSystem-like shim through a loaded Mach-O import.
5. Only then pursue Objective-C runtime bootstrap and a minimal unprotected own-app window.

Every claim must cite a test run and commit; no retail iOS application currently runs.