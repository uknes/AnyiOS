# Native ARM64 host track

Status: original, restricted native execution fixture **verified** on Windows ARM64 and Linux ARM64.

## Supported host CI

- `windows-11-vs2026-arm`: MSVC ARM64 native binary, `VirtualAlloc` RW → `VirtualProtect` RX → `FlushInstructionCache`, invoke owned AArch64 `MOVZ; RET` function. CI: https://github.com/uknes/AnyiOS/actions/runs/37748791787.
- `ubuntu-24.04-arm`: native AArch64 binary using `mmap` RW → `mprotect` RX → cache flush. Same CI.
- Both test that Clang-created, iOS-target ARM64 Mach-O **object** can provide exactly the approved 2-instruction function. CPU ISA and ABI agree for this deliberately trivial fixture.

## Shared and host-specific layers

```text
Owned ARM64 Mach-O file
  -> common format parser / dependency planner / dyld fixup staging
  -> shared guest-memory / Darwin API contracts
      -> Windows x64: Dynarmic ARM64-to-x64 translator
      -> Windows ARM64: native ARM64 execution (small owned proof only)
      -> Linux ARM64: native ARM64 execution (small owned proof only)
  -> host services and frameworks (NOT implemented)
```

The native proof is not general guest-code execution. Its public fixture function explicitly rejects all but the project-owned `MOVZ #42; RET` bytes, to avoid accidentally treating arbitrary iOS binaries as trusted host instructions. No Mach-O segment is mapped into host executable memory. A full native backend requires guest address-space isolation, signal/exception translation, Darwin syscall interception (an arbitrary iOS SVC must not reach the host kernel), and ABI adaptation.

## Runtime and security invariants

1. Do not share a host function pointer with arbitrary guest code; guest memory pointers need separate bounds and provenance.
2. Enforce write XOR execute and instruction-cache coherence before running even owned code.
3. Native ARM64 is not the same as iOS/Apple ABI compatibility. ObjC/Swift, dyld, Darwin traps, TLS, arm64e PAC, libraries and system frameworks remain non-portable.
4. Windows ARM64 host APIs are not interchangeable with Windows x86-64 ABI calls. Use an explicit host adapter boundary.
5. macOS Apple Silicon and Android ARM64 should be researched as follow-up host targets, not marked supported by these Windows/Linux tests.
6. Never execute unknown third-party Mach-O instructions in the interpreter's process without sandboxing or trust boundaries.

## Next acceptance criteria

- Create a common CPU execution contract with host-specific implementation, and ensure x64 and ARM64 runners test the same guest arithmetic and branch function.
- Prove guest guest-stack mapping and host-context transfer without unhandled Darwin SVC reaching Windows/Linux.
- Execute project-owned linked MH_EXECUTE and MH_DYLIB dependencies in isolated guest memory with verified relocation results.
- Use one versioned compatibility matrix across platforms and report unsupported functions explicitly.

## References

- https://docs.github.com/en/actions/reference/runners/github-hosted-runners
- https://github.blog/changelog/2026-08-20-windows-11-arm64-vs2026-image-generally-available/
- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect
- https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-flushinstructioncache
