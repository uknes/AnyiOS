# Bounded owned multi-image initializer execution

## Implemented contract

`plan_owned_image_initializers` builds a dependency-first constructor call list for an already staged, project-owned ARM64 executable/dylib graph. It reuses the strict canonical bundle-relative dependency planner. The entire graph and every descriptor/target are validated before returning a call list. Paths and function addresses in the returned plan are owned snapshots.

The graph is bounded to 256 images and 4096 constructor calls, with at most 64 constructor entries per image. Exactly one executable and a fully reachable graph are required. Each declared nonempty mapped segment must be live and readable (and executable where declared), with bounded nonoverlapping ranges. Duplicate/noncanonical paths, cycles, missing strong or weak dependencies, absolute system paths, external runpaths, upward dependencies and reexports are explicitly unsupported. The inherited path/depth/edge limits of `plan_dependencies` still apply. Metadata-only dependency discovery cannot authorize execution.

The existing single-image initializer reader now requires descriptor alignment, file-backed readable ownership, no overlapping descriptor tables, and a combined per-image count. Both `__mod_init_func` pointers and `__init_offsets` are supported in their documented named segments. A constructor must point to four aligned bytes within the same image's declared file-backed RX segment and be executable in live guest memory. An unrelated mapped RX page or rounded page padding no longer legitimizes a constructor pointer. This is a deliberately bounded contract; it does not claim support for arbitrary section-type variants or cross-image constructor pointers.

## Original compiler-built execution fixture

The SDK-free Clang/LLVM linker pipeline produces three ARM64 iOS Mach-O images from project-owned C sources:

1. `libInitLeaf.dylib`: its real constructor verifies guest argc/argv/envp/apple input and sets its value to 7.
2. `libInitMiddle.dylib`: its constructor calls the original leaf export and computes 73.
3. `InitializerChainApp`: its constructor calls the original middle export and computes 735; its actual LC_MAIN returns 735 only when both preceding initializations happened.

The fixture has no metadata-only system dependency. Chained imports resolve to real exports in the supplied original libraries. The existing multi-image mapping transaction stages all three files without modifying the input bytes. The call planner computes the order, then Dynarmic executes the actual constructor instructions through the existing bounded callback bridge. No guest output is fabricated, and no constructor state is set by host code.

Windows x64 CI runs:

    anyios-a64-smoke --initializers InitializerChainApp libInitMiddle.dylib libInitLeaf.dylib

Portable contract tests include a shared diamond dependency, dependency-first order independent of input order, snapshot lifetime, missing dependencies, cycles, unsupported edge kinds, orphan/duplicate images, alignment, overflow boundaries, zero-fill and out-of-image tables, overlapping tables, per-image limits, cross-image/nonexecuting/unaligned targets and file-backed code bounds. Windows ARM64 and Linux ARM64 execute the portable planning contracts natively. The initial implementation run exercised the three-image original instructions under Windows x64 Dynarmic; the additional native Windows ARM64 regression is described below.

The native Windows ARM64 trusted runner now also accepts `--initializers app middle.dylib leaf.dylib`. It stages the identical SDK-free three-image artifact using the shared import resolver, mapping transaction and initializer planner, publishes validated segments at their reserved native addresses with W xor X and executable SVC preflight, and copies the actual bounded guest argument vectors to nonexecutable pages. It calls the real original constructors and LC_MAIN; 735 is required. This uses the compatible fixed integer/pointer calling subset and the host call stack for these known freestanding C functions. It is a trusted CI fixture, not arbitrary native Darwin process execution, a guest-stack/exception bridge or a third-party app sandbox.

## Evidence and limits

Local GCC strict-warning builds and execution passed for `initializer_plan_tests` and the existing `process_bootstrap_tests`. The guest runner passes local C++ syntax checks. Windows x64 original owned compiled-guest execution passed at implementation head `827ce8e58d5c060d8ed2d7e4d7ed5f28c01199ce`, [CI 37974381605](https://github.com/uknes/AnyiOS/actions/runs/37974381605), job `windows-a64-translation` (113969039136), step `Execute three owned ARM64 images with dependency-first constructors`. The original guest constructors ran leaf/middle/main and the executable returned 735. All 14 main CI jobs passed; native Windows/Linux ARM64 portable planning contracts also passed. The existing constructor gate is marked **partial**, not verified full compatibility. Verified API/gate counts remain unchanged. Local ASan/UBSan passed with leak detection disabled because this executor prevents LeakSanitizer process inspection.

This is not arbitrary app startup, UIKit initialization, Objective-C +load, Swift runtime registration, concurrent/reentrant dlopen, unload/finalization or a guest scheduler. The callback bridge refuses SVC or exceptions rather than continuing initialization; completed constructor side effects are not rolled back. The call plan does not map modules, seal a namespace or maintain process-wide initialized state. A caller must execute a launch plan once and stop on failure.

Original Wikipedia/UIKitCatalog remain blocked by unresolved framework/dependency requirements. This milestone supplies a reusable initializer prerequisite without changing their input files or pretending to advance their entry trace.

## ABI research

- [Apple dynamic library design guidelines](https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html): dependent libraries initialize before their users.
- [Apple framework initialization](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPFrameworks/Tasks/InitializingFrameworks.html): constructor calling contract and library dependencies.
- [Apple dyld architecture](https://github.com/apple-oss-distributions/dyld/blob/main/doc/dyld4.md): image fixups, initialization and startup separation.

Implementation is independently authored. No Apple loader implementation is copied.
