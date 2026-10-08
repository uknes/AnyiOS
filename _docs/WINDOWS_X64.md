# Windows x86-64 execution roadmap

Status: Windows x64 executes project-owned ARM64 Mach-O object code and a restricted synthetic executable. Real linked iOS apps cannot run yet.
Research updated: 2026-10-08.

## Objective

Execute original and legally obtained ARM64 iOS code on Windows x86-64 through guest CPU translation plus Mach-O loading, Darwin API compatibility and independently implemented frameworks. The JIT is only one layer.

## Execution architecture

1. Binary analysis: the existing Mach-O inspector checks headers, universal ARM64 slices, segments, symbols and dyld metadata.
2. Guest memory: fixed-size, permission-checked guest virtual memory. Reject W+X pages, unaligned mappings, overlaps, cross-page access violations and arithmetic overflow.
3. Optional A64 JIT: Dynarmic translates ARM64 guest instructions into host x86-64 for Windows. A separately built test runs owned MOVZ x0,#42; RET machine code.
4. Loader: page-protected synthetic no-import MH_EXECUTE mapping with entry validation is implemented; realistic linked image layouts, dyld fixups and import resolution remain unsupported.
5. Experimental runtime boundary: a bounded Darwin guest SVC #0x80 bridge provides captured write/exit behavior for owned tests; Mach IPC and full libSystem are unimplemented.
6. Planned frameworks: libSystem, CoreFoundation/Foundation, ObjC/Swift metadata and messaging, UIKit, audio/graphics/input. No framework works yet.

Guest addresses must never be dereferenced as native host pointers. Native shims validate/copy byte ranges and explicitly account for guest virtual memory and thread state.

## Dependency governance

Optional Dynarmic source pinned at a46601580d5512d324104f985b5f0209dc980ddc; license: 0BSD. It is fetched by CMake only when ANYIOS_WITH_DYNARMIC is ON. Windows CI uses Boost 1.87.0 header package via NuGet. Keep each dependency's notices and license requirements under review before distributing binaries.

FEX's normal direction is x86-64 -> ARM64, unlike our guest ARM64 -> host x86-64 direction. The GPL-2.0 Unicorn implementation must not be copied into this MIT repository without a deliberate license review.

## Reproduce Windows CPU proof

    cmake -S . -B build-jit -DANYIOS_WITH_DYNARMIC=ON -DBUILD_TESTING=ON
    cmake --build build-jit --config Release --target anyios-a64-smoke
    ctest --test-dir build-jit --build-config Release -R arm64-on-x64 --output-on-failure

Build prerequisites: supported C++20 toolchain, CMake, Git and Boost headers, plus internet on first dependency fetch. Existing inspector build remains dependency-free.

## Milestone acceptance

- First: Windows x86-64 CI executes the owned two-instruction ARM64 program, asserts x0=42 and return PC; no guest memory faults.
- Second: test original compiled ARM64 Mach-O object section execution, not pre-embedded instruction words; validate section/relocation consistency and error handling.
- Third: implement actual MH_EXECUTE mapping, dyld imports, guest stack and a small owned program calling an explicitly implemented host bridge.
- Fourth: initialize Objective-C runtime and an owned app's lifecycle; then graphics and input.

## Important limitations

Modern apps may depend on arm64e pointer authentication, Metal, proprietary frameworks and Apple service entitlements. No claim that AnyiOS launches retail apps or replaces a real iOS device is warranted. No DRM bypass or Apple copyrighted binaries.

## References

- https://github.com/azahar-emu/dynarmic
- https://github.com/touchHLE/touchHLE
- https://github.com/darlinghq/darling
- https://github.com/apple-oss-distributions/dyld
- https://developer.apple.com/documentation/security/preparing-your-app-to-work-with-pointer-authentication
- https://www.nuget.org/packages/boost/1.87.0
