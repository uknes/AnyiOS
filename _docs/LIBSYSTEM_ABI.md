# Owned libSystem ABI contracts (limited compatibility)

Status: **implemented on feature branch, CI pending**. No general Darwin libc, POSIX, pthread or Objective-C compatibility is claimed.

## Host/guest boundary

All guest calls originate from original, project-owned SDK-free Clang-generated `arm64-apple-ios` executables. A registered import resolves to a guest RX thunk with `mov x16, #service; svc #0x80; ret`. Windows x64 Dynarmic catches this event, validates the thunk's guest PC and x16 marker, and passes the three x0–x2 words to `LibSystemShim::invoke`; return values are placed in x0. Only guest memory is read or written, never raw guest pointers cast as host pointers. Native Windows ARM64 SVC stays refused.

All functions here return an explicit exception or unsupported event for ranges, unmapped buffers or missing contracts. No success stubs.

| Guest import | Apple arm64 owned-fixture contract | Return | Defensive boundary |
| --- | --- | --- | --- |
| `_memcpy` | x0=guest dst, x1=guest src, x2=`size_t` (64-bit) | guest destination address in x0 | Copy requires fully readable src, writable dst, length ≤64 KiB, no overlap; length 0 accesses no memory |
| `_memset` | x0=guest dst, **w1=`int`** (only low 8 bits fill), x2=`size_t` | guest destination in x0 | Length ≤64 KiB and fully writable; length 0 accesses no memory |
| `_strlen` | x0=guest pointer to NUL-terminated char bytes | 64-bit `size_t` in x0 | Refuse after 64 KiB without NUL; every byte must be guest-readable |
| `_strcmp` | x0,x1=guest pointers to NUL-terminated unsigned char sequences | `int` sign in **w0** (32-bit; returns -1/0/+1) | Refuse unmapped buffers or 64 KiB without termination/mismatch; compare unsigned bytes |

All 64-bit returned pointers are guest virtual addresses, never host addresses. The C ABI only requires the **sign** of nonzero `strcmp` results; exact byte subtraction is not guaranteed by this subset. Tests inspect w0 for a negative return and explicitly check the low 8-bit truncation of `memset`'s 32-bit integer argument.

**Variadic boundary:** these four functions are **not variadic**, and no arbitrary vararg forwarding is implemented. Existing `apple-arm64-fixed-abi` regression covers stack-passed variadics and small integer extension in isolation, not a working `printf` contract. Before `printf`/`fprintf` support, implement and test Apple arm64 stack variadic layout, format parsing with a strict whitelist, read-only format pointer validation, negative widths/precisions, and refusal of unsupported specifiers/pointer callbacks. Do not pass guest va_list to host libc.

## Outstanding requested libSystem scope

- `___error` / errno: per-guest-thread lifetime and pointer ABI still need an owned test. Do not reuse host `errno` or invent a global guest pointer.
- `_abort`: explicit guest process termination signaling, not host `abort()` (which kills the runner).
- `_puts`, `_fputs`, `_printf` and basic stdio: write-backed only with bounded, format-aware guest memory access. Neither `FILE*` nor host variadics may cross unchanged.
- `_pthread_once`, `_pthread_mutex_lock`, `_pthread_mutex_unlock`: single-thread owned fixtures first; strict lifecycle, initializer values and recursive lock behavior must be specified. Threads/scheduler/guest TLV destructors remain unsupported.

## Acceptance

- Bounded host contract CTest: `minimal-libsystem-host-contract`, with out-of-range, overlap, unmapped and low-int-width negative checks.
- Actual SDK-free Clang iOS ARM64 `MemoryStringApp` imports, checked by `sdkfree_link.py`, then execute on Windows x64 Dynarmic and verify LC_MAIN returns **27**.
- All supported-platform CTest matrix and `windows-a64-translation` job green before marking this verified in `STATE.md`.

## Original Wikipedia execution acceptance

Prior exact source-built unchanged Wikipedia trace in 37914916684 handled
os_log_type_enabled at instruction 53 and stopped at _memcpy at instruction 74.
The general entry probe now calls this actual bounded memory-copy implementation
with original x0/x1/x2, returns the original guest destination in x0, and retains
explicit failure for unmapped, protected, overlapping or excessive ranges.
The Wikipedia workflow requires SUPPORTED_NARROW_IMPORT=_memcpy followed by a
different actual blocker, with original executable SHA verification, before
acceptance. No original application source/binary changes. The fixed 64 KiB cap
is a research limitation; do not claim complete memcpy/libc or promote API/gate
inventories from this subset. Tests also prove no partial destination write when
a source is unreadable or a destination crosses an unmapped boundary.
Reference: https://pubs.opengroup.org/onlinepubs/9699919799/functions/memcpy.html
