# SDK-free Mach-O linker experiment

Status: experimental, not yet CI-verified. Date: 2026-10-08.

## Purpose

Prove that LLVM Clang + ld64.lld can produce original ARM64 iOS Mach-O MH_EXECUTE / MH_DYLIB fixtures on Linux, using **no Apple SDK**. The test generates its own TAPI v4 text stub with the libSystem install name. It contains no Apple code or implementations and cannot fulfill runtime calls.

## Ingredients

- Original project-owned C fixtures: `arm64_linked_app.c`, `arm64_widget.c`
- `tests/sdkfree_link.py` (stdlib only), LLVM `clang-19` and `ld64.lld-19`
- `anyios-inspect` for strict load-command checks
- Linux GitHub Actions job `sdk-free-ios-link`

Failure is actionable: a linker that rejects iOS, the TAPI stub, or emitted Mach-O is recorded as unsupported for that toolchain. No test is skipped or converted to a false success.

## Notes

LLD's Mach-O port documents support for Apple ld64-like command-line options: https://lld.llvm.org/MachO/index.html. The LLVM project license at pinned surveyed SHA `7ac405140aff8a754d5d669f8452d8e4c207f8cc` is Apache-2.0 WITH LLVM-exception; license reviewed on 2026-10-08 via `LICENSE.TXT`.

Even if this test passes, SDK-free **linking** is not a libSystem implementation and proves no GUI/runtime support. The existing macOS linker fixture remains the reference until parity is established.
