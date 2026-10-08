# AnyPS5 re-audit — 2026-10-08

Review target: boykopovar/AnyPS5 at commit df16c4c256be3c44e03eb9149a6ce5f8e8a038b2.
AnyiOS target: Windows x86-64 host, ARM64 Mach-O guest, clean-room runtime and API compatibility.

## Transferable engineering patterns

1. Guest memory and Windows process isolation: [Windows address-space integration test](https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_windows_address_space.py) checks loader flags, address-space assumptions and actual Windows execution. AnyiOS must test guest/host isolation and guard pages before adding large memory arenas.
2. Segment mapping: [executable-segments tests](https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_executable_segments.py) and [load-alignment tests](https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_linux_load_alignment.py) check multiple segments, alignment and executable permissions. AnyiOS adopted original strict `__TEXT` header placement, file overlap rejection, max/init protection consistency, two-segment data and BSS tests, and atomic rollback when a segment fails.
3. Module and symbol resolution: [Windows guest-import integration tests](https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_windows_import_modules.py) differentiate exact module matches from ambiguous aliases. In AnyiOS, dyld install names, @rpath scope, two-level namespaces, weak imports and symbol binding will need deterministic resolution and explicit ambiguity diagnostics.
4. Thread-local storage: [Windows guest TLS tests](https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_guest_tls.py) verify different loaded modules do not share thread-local state. This should become a separate AnyiOS runtime contract milestone after dyld mapping.
5. Strict relocations: [absolute-symbol regression tests](https://github.com/boykopovar/AnyPS5/blob/main/core/relinker/relinker/tests/test_guest_absolute_symbols.py) check relocation results and unsupported symbol sections. AnyiOS must validate the exact Mach-O arm64 relocation and chained-fixup kind and fail unsupported modes rather than assuming they are equivalent.
6. Error boundaries: AnyPS5 keeps system library implementations separate from relinking and exercises malformed file rejection in dedicated negative tests. AnyiOS maintains binary parser, guest memory, Windows translation backend, and future Darwin shims as independent layers.

## What does not transfer

- AnyPS5's ELF/PRX conversion uses x86-64 guest code that can run natively; AnyiOS requires an ARM64-to-x86-64 JIT before any guest instructions execute.
- PS5 NIDs, ELF relocation rules, AMD instruction rewrites, AGC and RDNA shader pipelines, PRX shims and executable PE patching are not substitutes for Mach-O, dyld, Objective-C, UIKit, Swift or Metal compatibility.
- GPL-2.0 source from AnyPS5 is not copied into this MIT-licensed project. File layout, concepts and test *objectives* are studied and reimplemented independently.

## Apple-specific source of truth

- [Apple dyld MachOAnalyzer](https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOAnalyzer.cpp) validates load-command coverage in `__TEXT`, segment layout, imports, entry points and linkedit structures.
- [Apple Mach-O ABI headers](https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/loader.h) define the real segment permissions and LC_MAIN metadata. Tests in this repository use original synthetic bytes and project-owned compiled code.

## Next tests (not yet implemented)

- Relocatable MH_DYLIB and independently linked MH_EXECUTE fixtures; do not equate a synthetic executable with a fully linked app.
- Multiple modules with duplicate symbols, strict install-name resolution, weak references and missing-library diagnostics.
- Guest TSD/TLS isolation across threads and mapped modules, guest stack and Darwin calling conventions.
- Windows host address-space constraints, repeated JIT execution and non-executable host/guest pages, plus fuzzing of new loader boundaries.
- Explicit Darwin SVC and Mach trap diagnostics; no success stubs for unsupported syscalls.

## Evidence policy

Code and GitHub CI determine the actual implementation. A fully functional iOS application on Windows x86-64 is not yet supported. This review is research, not claimed compatibility.