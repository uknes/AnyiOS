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
