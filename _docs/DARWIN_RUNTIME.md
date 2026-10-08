# Darwin guest trap and dyld dependency foundations

Status: working experimental modules; complete iOS runtime not implemented.

## ARM64 guest-to-Windows boundary

Sources: [Apple XNU syscall table](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/syscalls.master), [Darling system-call emulation](https://docs.darlinghq.org/internals/basics/system-call-emulation.html), [Dynarmic A64 callbacks](https://github.com/azahar-emu/dynarmic/blob/master/src/dynarmic/interface/A64/config.h).

Project code: `include/anyios/darwin_syscall.hpp`, `src/darwin_syscall.cpp` and a bounded `GuestMemory::copy_from` operation. In the test JIT, `SVC #0x80` becomes a recorded event, and register values are read **after** a Dynarmic Step. Only write(4) to stdout/stderr buffers and exit(1) signaling are modeled. Unknown SVC immediates/syscall numbers fail explicitly. Errors use selected Darwin errno values and condition flags; ABI coverage is incomplete.

Security constraints: no raw guest pointer may be dereferenced on the host, writes are at most 64 KiB per call, aggregate captured output at most 1 MiB, and no arbitrary Windows file descriptor is exposed. This code is not a host OS syscall passthrough.

The Windows CI compiles `tests/fixtures/arm64_syscall.S` as a real ARM64 Mach-O object using Clang, then executes its SVC through Dynarmic. This verifies one guest-to-host write boundary when CI passes; it does not bootstrap libSystem or Mach IPC.

## dyld graph planner

Source material: [Apple dyld @rpath guidance](https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/RunpathDependentLibraries.html) and [dyld manual](https://keith.github.io/xcode-man-pages/dyld.1.html).

`include/anyios/dyld_plan.hpp`, `src/dyld_plan.cpp` use parsed Mach-O images in a **pre-registered, in-memory module map**. Canonical guest-relative paths are required. The planner expands @loader_path and @executable_path, evaluates @rpath candidates in ordered inherited runpath contexts, skips explicitly missing weak dependencies, and rejects unregistered strong dependencies, cycles, invalid guest paths, and unsupported system/library names. It returns dependency-first order for a strict subset.

**Not a dynamic loader:** no Windows host filesystem scanning, platform dyld shared cache, ELF/PE conversion, binding, symbol lookup, reexports, lazy linking, bundle installation, framework implementation or real app loading. The runpath search model is intentionally narrower than Apple's full dyld resolution semantics.

## Explicit next proof

1. Build a linked owned MH_EXECUTE + MH_DYLIB pair, inspect its real LC_RPATH/LC_LOAD_DYLIB and fixups.
2. Validate relocations and mapping atomically with negative tests.
3. Tie dyld import targets to a small audited Darwin function shim, execute from real linked code on Windows x64.
4. Expand system call and ObjC framework support only after observable tests.