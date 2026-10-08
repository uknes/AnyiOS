# Architecture

## Scope

An ARM64 iOS binary cannot execute on ARM64 Android/Linux merely because CPU instructions match. Mach-O loading, dyld linking, Darwin ABI, Objective-C/Swift and frameworks still need compatible implementations.

## Implemented

ARM64 thin Mach-O file -> host file reader -> pure bounded metadata parser -> immutable Image result -> CLI diagnostics.

- include/anyios/macho.hpp: public parser types and entrypoint.
- src/macho.cpp: safe command parsing and range validation.
- tools/inspect.cpp: host file I/O and diagnostics only.
- tests/macho_tests.cpp: self-produced valid and corrupted binary fixtures.

Unknown command types are skipped only after verifying their command sizes; this does not indicate runtime support.

## Proposed future boundaries, none implemented

- format: binary parsing and bundle inspection; no host API calls.
- loader: validated memory mappings, relocations, imports, TLS and exceptions.
- runtime: Darwin ABI, syscalls/Mach IPC, libSystem, ObjC/Swift contracts.
- frameworks: Foundation, UIKit, graphics, audio/input.
- host adapters: Linux ARM64 first; Android, Windows ARM64 and x86 JIT only after feasibility evidence.

No empty framework stubs pretending to be supported. Use reproducible narrow vertical slices. Untrusted offsets and sizes are checked before use. Avoid importing GPLv2-only AnyPS5 implementation into MIT code.


## M1 implemented module boundaries

- src/universal.cpp: FAT/FAT64 input validation, endian decoding, slice selection and metadata; dispatches only to the thin parser.
- src/macho.cpp: thin ARM64 load-command metadata scanning; delegates complex linkedit payloads to separate functions.
- src/linkedit.cpp: bounded dyld chained-import and export-name inspection; no runtime relocation performed.
- src/symbols.cpp: independent bounded nlist_64 static symbol name/value/type extraction.
- src/macho.cpp: validates section_64 boundaries and indirect symbol table ranges; no relocation execution.
- src/internal.hpp: private inter-module interfaces. The public metadata API remains include/anyios/macho.hpp.
- tests/m1_tests.cpp and tests/real_fixture.py: systematic negative inputs and original cross-compiled object verification.
- tests/fuzz_macho.cpp: standalone instrumentation (not an uninstrumented static-library wrapper).

No code maps guest executable pages or calls an entry point. Important future proof obligations include decomposed CPU subtype/capabilities, fixup pointer-format correctness, ObjC metadata ABI and signed executable constraints.
