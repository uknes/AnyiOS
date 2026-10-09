# Safe read-only Apple legacy dyld eager BIND opcode scanner

Built from Apple's open dyld MachOAnalyzer, the bounded
`inspect_legacy_eager_bind_sites()` planner reads the ORIGINAL
LC_DYLD_INFO regular bind opcode range without touching the Mach-O,
guest memory or host imports. It records file offset, original VM
address, symbol name, dylib ordinal, addend and weak-import flag.
Supports ordinary 64-bit BIND_OPCODE_SET_DYLIB_ORDINAL_IMM/ULEB,
SET_DYLIB_SPECIAL_IMM, SET_SYMBOL_TRAILING_FLAGS_IMM, SET_TYPE_IMM,
SET_ADDEND_SLEB, SET_SEGMENT_AND_OFFSET_ULEB, ADD_ADDR_ULEB,
DO_BIND, DO_BIND_ADD_ADDR_ULEB, DO_BIND_ADD_ADDR_IMM_SCALED,
DO_BIND_ULEB_TIMES_SKIPPING_ULEB, DONE.

Unsupported lazy/weak/threaded opcodes, malformed ULEB/SLEB,
symbol strings >255 bytes, missing required binding state,
unmapped/unaligned/non-writable/executable targets, duplicate
sites and >65536 bindings fail closed. There is no dyld linker,
symbol implementation, native execution, guest UIKit visuals,
host pointers or patch application in this milestone.

Next: supported lazy & weak binds, correct per-image ordinal
namespace and guest-framework resolution, atomic rebase/bind
patches with slide, then try exact unchanged original UIKitCatalog
Apple ARM64 Mach-O under Windows x64 Dynarmic.

Public dyld source: https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOAnalyzer.cpp

## Exact unchanged original iPhoneOS UIKitCatalog opcode inventory regression

The original native Windows ARM64 metadata stage now performs a **read-only**, bounded parse of the same Xcode-built original UIKitCatalog ARM64 binary's legacy rebase and eager-bind streams, with its SHA-256 separately verified. It records `LEGACY_REBASE_SITES` / `LEGACY_EAGER_BIND_SITES` and the first 64 distinct binding symbol names; unsupported real-world opcodes or invalid ranges are reported as `LEGACY_REBASE_DECODER_BLOCKER` / `LEGACY_BIND_DECODER_BLOCKER`. A completed metadata report is still followed by the loader's required legacy dyld refusal; there is no patching, importing private Apple frameworks, ARM64 guest instruction execution or original UIKit pixels. The original-app CI checks that each present stream produced either an actual bounded site count or a specific fail-closed decoder diagnostic.
