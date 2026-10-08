# SDK-free Mach-O linker experiment

Status: **verified on Ubuntu 24.04 Linux** (the Windows SDK-free linker path is not yet tested). Date: 2026-10-08.

## Purpose

Prove that LLVM Clang + ld64.lld can produce original ARM64 iOS Mach-O MH_EXECUTE / MH_DYLIB fixtures on Linux, using **no Apple SDK**. The test generates its own TAPI v4 text stub with the libSystem install name. It contains no Apple code or implementations and cannot fulfill runtime calls.

## Ingredients

- Original project-owned C fixtures: `arm64_linked_app.c`, `arm64_widget.c`
- `tests/sdkfree_link.py` (stdlib only), LLVM `clang-19` and `ld64.lld-19`
- `anyios-inspect` for strict load-command checks
- Linux GitHub Actions job `sdk-free-ios-link`

The Ubuntu LLVM-19 test has passed in [run 37759173521](https://github.com/uknes/AnyiOS/actions/runs/37759173521), job `sdk-free-ios-link`: it links original project-owned code and verifies real Mach-O dependency metadata with the inspector. The file is a linker-only metadata stub; its existence cannot satisfy libSystem functions at runtime. Any future linker failure is actionable and must not be skipped.

## Notes

LLD's Mach-O port documents support for Apple ld64-like command-line options: https://lld.llvm.org/MachO/index.html. The LLVM project license at pinned surveyed SHA `7ac405140aff8a754d5d669f8452d8e4c207f8cc` is Apache-2.0 WITH LLVM-exception; license reviewed on 2026-10-08 via `LICENSE.TXT`.

Even if this test passes, SDK-free **linking** is not a libSystem implementation and proves no GUI/runtime support. The existing macOS linker fixture remains the reference until parity is established.

## Added original libSystem import fixture

The authored `tests/fixtures/arm64_libsystem_app.c` imports `malloc`, `write`, and `exit` from the authored metadata-only TAPI stub. The Linux `sdk-free-ios-link` job verifies those imports and libSystem's install name; this worked in CI run 37762837584. `LibSystemShim` implements a bounded guest bump allocator (null on exhaustion), captured writes, and guest exit signaling **in host unit tests**. No Apple runtime or proprietary SDK binary is distributed.

A separate Windows x64 Dynarmic integration test now tries to execute those linked imports through registered ARM64 SVC thunks. It must **not** be marked verified until the specific guest execution CI step passes.
