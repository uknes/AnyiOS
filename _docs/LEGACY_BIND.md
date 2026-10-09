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
