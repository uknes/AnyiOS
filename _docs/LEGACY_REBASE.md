# Safe Apple ARM64 legacy dyld rebase bytecode scanner

Read-only `anyios::dyld::inspect_legacy_rebase_sites` recognizes the
complete official rebase opcode family (DONE, SET_TYPE, SET_SEGMENT,
ADD_ADDR, DO_REBASE_IMM/ULEB, DO_REBASE_ADD, SKIPPING_ULEB) with
overflow-resistant ULEB bounds, original segment indices, file-backed
8-byte pointer alignment/ranges, duplicate target detection and 65,536
site cap. It accepts **only** 64-bit pointer rebase type1. The image
remains untouched; the output is a list of original file offsets and
VM addresses and does NOT apply guest slide, mutate guest memory or
resolve imported bind/lazy-bind opcodes.

The original UIKitCatalog executable's legacy opcode format is a known
first loader blocker. This is **metadata interpretation only**, not
application execution. Next, implement checked lazy/eager bind opcodes,
calculate guest slide, stage patches transactionally without W+X, then
retest original SHA-pinned UIKitCatalog image on both Windows hosts.

Official format sources:
https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOAnalyzer.cpp
https://github.com/llvm/llvm-project/blob/main/lld/MachO/SyntheticSections.cpp
