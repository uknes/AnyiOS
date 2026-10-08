# M2 — ARM64 iOS binary execution feasibility

Status: Research only, 2026-10-08. There is **no** executable-loader implementation.

## Goal

Prove that original, freely redistributable, unprotected code compiled as a *real* ARM64 Mach-O executable can be mapped, relocated and invoked in a controlled non-Apple ARM64 host process, or document a falsified assumption that blocks it. An MH_OBJECT is a compiler object, not an iOS app and not sufficient for this milestone.

## Compatibility challenges to resolve separately

1. Mach-O layout: segments, virtual memory protection, ASLR, page alignment, LC_MAIN/LC_UNIXTHREAD, exported symbols and chained fixup pointer formats.
2. dyld dependencies: @rpath, @executable_path, @loader_path, module initialization, weak references, TLS, rebases and binds. Parsing import names does not implement dynamic linking.
3. Host ABI: Darwin on ARM64 has distinct syscall conventions, Mach facilities and process/thread behaviors from Linux/Android; matching AArch64 instructions alone is insufficient.
4. Objective-C and Swift: metadata formats, classes/categories, ObjC message sends, blocks, Swift runtime and system framework APIs. Evaluate GNUstep libobjc2 only as a research input; ABI compatibility must be demonstrated with owned compiled fixtures.
5. iOS security model: signing, entitlements, hardened-runtime assumptions and arm64e pointer authentication. No DRM circumvention or proprietary system binaries.
6. App lifecycle and display: application services, Foundation/UIKit, compositor events and graphics are independent stages, not prerequisites for a command-line test.

## Evidence-based experiments

- Build owned thin MH_OBJECT fixtures with Clang (already verified).
- Build owned MH_EXECUTE/MH_DYLIB artifacts with a suitable verified linker/toolchain. The available local ld64.lld rejected the tested platform combinations; this attempt is not a linked-binary success.
- Compare file metadata and exported/imported symbols using LLVM tools.
- Specify memory map permissions, address slides and relocations with synthetic fixtures and page-level tests.
- Define explicit host boundary functions and controlled failure mode before any guest execution.
- Run an owned executable under a restricted ARM64 environment only after a security review.

## Stopping rules

No positive compatibility claim based solely on metadata inspection or object compilation. No pseudo-framework APIs that return success without correct behavior. No copied Apple proprietary implementations or AnyPS5 GPL code.

## References

- https://github.com/apple-oss-distributions/dyld
- https://github.com/gnustep/libobjc2
- https://github.com/darlinghq/darling
- https://clang.llvm.org/docs/CrossCompilation.html
- https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/loader.h
