# Architecture decisions

## ADR-001 — Inspection before execution

Accepted 2026-10-08. Verify format and boundary behavior before loading executable guest code. Matching ARM64 CPUs do not solve Darwin ABI or framework requirements.

## ADR-002 — C++20 and zero dependencies for M0

Accepted 2026-10-08. CMake and portable C++20 spans enable host-independent parser tests. Any dependency later requires license, security and build justification.

## ADR-003 — Parser / loader / runtime / framework separation

Accepted 2026-10-08. Only parser exists; future modules cannot claim support from unimplemented stubs. Parser reports metadata, not executable readiness.

## ADR-004 — Clean-room and lawful input

Accepted 2026-10-08. Public format descriptions and owned fixtures are allowed. Proprietary binaries, firmware, cryptographic keys and DRM circumvention are excluded. AnyPS5 is GPLv2-only: review its patterns but copy none of its source.

## ADR-005 — Git and CI over project claims

Accepted 2026-10-08. Notion tracks tasks and research, but source history, versioned state, actual tests and CI decide whether something works.


## ADR-006 — FAT container and dyld metadata are separate from execution

Accepted 2026-10-08. Select ARM64 from a bounded universal wrapper and parse each thin image as a subspan. Verify table/slice overlap, alignment and matching subtype. Chained import and export analysis returns diagnostic names only, not relocations or native linking.

## ADR-007 — Untrusted input resource ceilings

Accepted 2026-10-08. Cap arch count at 4096, load command count at 16384, chained imports at 100000, per-name length at 16384 bytes, aggregate imported name data at 8 MiB and exported trie nodes at 65536. Limits are defensive policy and are not file-format specifications.

## ADR-008 — 16 KiB iOS arm64 guest pages, 4 KiB private backing granules

Accepted 2026-10-08. Apple documents that 64-bit iOS userspace exposes 16 KiB virtual-memory pages. AnyiOS therefore treats **16,384 bytes as the guest ARM64 process page size**. `GuestMemory::map_ios` enforces 16 KiB address/size alignment, while its existing private 4 KiB backing granules remain an implementation detail for legacy synthetic fixtures and host-independent bounds testing. A real linked-image load must use `map_ios` and must never grant different permissions to adjacent 4 KiB backing granules inside one iOS guest page. This explicit dual-granularity migration prevents silently treating a host Windows 4 KiB page as an iOS page. The synthetic parser/unit fixture path remains allowed to use 4 KiB backing pages temporarily; it is **not a conforming iOS process mapping**.

References: https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/AboutMemory.html and https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms.

## ADR-009 — Journal only newly mapped pages, no 64 MiB guest clone

Accepted 2026-10-08 after introducing `GuestMemory::MappingJournal`. Loader mappings are recorded by guest address and range, data initialization may only modify pages just mapped by that journal, and failure clears page bytes/permissions in reverse order. The owned two-image loader shares one journal for atomic rollback across both images. The original 4 KiB synthetic parser fixtures remain isolated, while real linked-image staging requests 16 KiB guest-aligned mappings. Verification: `guest-memory`, `macho-linked-image-stage`, `macho-static-executable` and real Windows artifact staging jobs in GitHub Actions. Treat full-run success as pending until the latest CI concludes.

## ADR-010 — Native ARM64 code always needs an ABI isolation boundary

Accepted 2026-10-08. Running iOS ARM64 instructions on a Windows ARM64 CPU is not the same as the Apple ABI or Darwin runtime. The native test adapter denies arbitrary opcodes and SVCs. The trusted in-process linked test is an exception for project-owned, CI-generated code only, never a production loader. A future host libSystem call requires a signature-aware thunk layer, host x18 protection, Apple variadic/small-integer conventions, guest page guards and process isolation. See [ABI_BRIDGE.md](ABI_BRIDGE.md).

## ADR-011 — Strict iOS pages with final LINKEDIT padding

Accepted 2026-10-08 after focused CI tests. Every iOS user-code and data mapping requires 16 KiB guest virtual page alignment. The LLVM Mach-O linker can emit a *final, read-only* `__LINKEDIT` metadata segment with a shorter declared VM extent. The loader may round **only this last read-only metadata mapping** to the next full 16 KiB page and must reject an unaligned base, nonfinal LINKEDIT, executable/writable short segments, overlaps or out-of-range padding. Padded bytes remain zero and read-only. The original 4 KiB `__TEXT` regression still rejects. Verified SDK-free staging on Windows x64/ARM64 and synthetic CTest: https://github.com/uknes/AnyiOS/actions/runs/37762837584.

## ADR-012 — Start with explicit host-side libSystem shims; do not port guest libc yet

Accepted 2026-10-08. AnyiOS remains MIT. Two options were evaluated and **no external libc source was copied**.

### Option A: signature-aware host-side libSystem shims

The current design resolves explicitly approved dyld symbols to bounded ARM64 guest thunks. A Windows-x64 Dynarmic SVC event invokes a host implementation that operates solely on guest memory. Pros: tiny trusted API surface, deterministic tests, precise unsupported diagnostics, inspectable permissions, and no new bundled third-party libc dependency. Cons: each API requires a compatible Apple ARM64 contract (narrow integer extension, Apple stack-passed variadics, return values, pointer copy-in/out, errno, callbacks and indirect blocks), plus Mach/dispatch/TLS semantics. Windows ARM64 cannot directly enter host functions by jumping from arbitrary Apple code: a separate isolated process and native ABI thunk must protect x18, callee-saved registers and stack.

### Option B: compile a guest-side, permissively licensed libc to arm64-apple-ios

**musl** — verified repository `kraj/musl`, pinned commit `5122f9f3c99fee366167c5de98b31546312921ab` (commit date 2026-04-10); read actual `COPYRIGHT` on 2026-10-08. Project-wide MIT, with documented permissively licensed individual portions. The source targets the **Linux** syscall and ELF/libc startup contract, so targeting arm64-apple-ios is not a CFLAGS switch: Mach-O/TAPI linking, Darwin syscalls and Mach IPC, guest TLS/TPIDRRO_EL0, TLV descriptors, pthread/dispatch, signals, errno and allocator/thread registration must be reworked.

**FreeBSD libc** — verified repository `freebsd/freebsd-src`, pinned commit `c2b7fe4a9e94a0edba9dd2772874928b565c4f9e` (commit date 2026-10-08); read actual top-level `COPYRIGHT` on 2026-10-08. Compilation under BSD-2-Clause; individual source files can have different copyright/license terms, so **a separate file-by-file audit is required before adoption**. A BSD syscall/ABI heritage may be somewhat closer to Darwin POSIX names, but Darwin Mach traps, Apple arm64 calling conventions, dyld TLS and Apple process startup still differ. FreeBSD's guest-side libc cannot simply call the Windows kernel.

A single guest syscall primitive would simplify selected `write`/`exit` APIs under Dynarmic (SVC callbacks); it would **not** solve allocator thread-safety, TLS, block callbacks or variadic libc API ABI compatibility, and it cannot run natively on Windows ARM64 before isolated SVC trapping. Guest libc would still depend on a host Mach/Darwin emulation layer for nontrivial applications.

**Decision: Option A for the first hello-world and basic libSystem subset.** Keep the door open for selectively linked, separately audited MIT/BSD guest-side implementations only after real TLS, threads and syscall contracts are measurable. Do not fork/copy musl or FreeBSD libc or claim libc compatibility today. Keep the original guest program and ABI regression tests as the oracle.

References (license files verified at those commits):
- https://github.com/kraj/musl/blob/5122f9f3c99fee366167c5de98b31546312921ab/COPYRIGHT
- https://github.com/freebsd/freebsd-src/blob/c2b7fe4a9e94a0edba9dd2772874928b565c4f9e/COPYRIGHT
