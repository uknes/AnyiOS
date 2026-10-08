# Owned libSystem C memory primitives — ABI contract

Scope: **owned SDK-free Clang iOS fixtures only**. This is a host-side ABI contract,
not a binary-compatible Darwin libc, not concurrent and not a generic iOS app runner.

Status: source and portable tests implemented; **CI verification pending**.
Source: `src/libsystem_shim.cpp`, test: `tests/libsystem_shim_tests.cpp`.

## Guest ABI and bounds

| Export | ARM64 guest arguments | Return (x0/w0) | Fail-closed behavior |
| --- | --- | --- | --- |
| `_memcpy` | x0=guest destination, x1=guest source, x2=size_t count | x0=original guest destination | Unsupported overlap, unreadable source/unwritable destination, count >64KiB: throws |
| `_memset` | x0=guest destination, w1=int fill, x2=size_t count | x0=original guest destination | Fill is truncated to unsigned char; inaccessible destination or count >64KiB: throws |
| `_strlen` | x0=guest const char* | x0=size_t byte count | Missing accessible NUL in first 64KiB, overflow, unmapped read: throws |
| `_strcmp` | x0/x1=guest const char* | **w0**, 0, -1 or +1 encoded in low 32 bits | Missing accessible NUL in first 64KiB or unmapped read: throws |

- All read/write addresses refer to `GuestMemory`; never dereference them as host addresses.
- For `memcpy`/`memset`, a zero byte count returns x0 without dereferencing either address. This is an owned fixture contract only.
- A nonzero byte count is fully range-checked *before* guest destination mutation. `memcpy` uses a temporary owned host buffer after validating both guest spans; it is not guest `memmove`.
- `strcmp` compares **unsigned 8-bit C characters** and returns only sign (not raw difference), which is valid C strcmp behavior. Its result is a signed C `int` in low `w0` bits. Upper 32 bits are not part of the contract.
- ABI specifics: guest `size_t` is 64-bit; `int` is 32-bit; argument promotion/sign/zero extension is explicitly narrowed where needed. No variadic argument is interpreted by these four APIs.
- The `_write`, `_exit`, `_malloc` contracts remain unchanged. `errno`, `abort`, stdio, `pthread_once`, mutex, callbacks and variadic stdio still require separate fixtures and implementation: **not implemented** by this change.

## Tests

Portable narrow-int and pointer tests include high bits in `memset`'s fill argument,
negative `strcmp` low `w0` result, zero-byte calls, pointers outside mapped pages,
overlap refusal, over-budget length and destination integrity after a refused call.

Do **not** update `_docs/STATE.md` until CI verifies the exact source commit and test step.
