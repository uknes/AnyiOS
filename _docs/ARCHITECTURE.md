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
