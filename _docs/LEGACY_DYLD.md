# Original UIKitCatalog legacy dyld format — recognition milestone

Exact upstream Appium/UIKitCatalog ARM64 binary is source-verified,
built unmodified, and refused by both AnyiOS link staging and Dynarmic
before entering guest code in [CI 37870715261](https://github.com/uknes/AnyiOS/actions/runs/37870715261).

- Windows native ARM64 stage: STATIC_IMPORTS=0, STAGING=blocked,
  FIRST_LOADER_BLOCKER=unsupported linked image legacy dyld/thread state.
- Windows x86-64 Dynarmic: ENTRY_PROBE=not-started,
  FIRST_LOADER_BLOCKER=unsupported external app chained import count.

The loader intentionally supports *modern chained fixups only*. Apple's
original dyld documentation and LLVM agree LC_DYLD_INFO /
LC_DYLD_INFO_ONLY have separate bounded rebase, bind, weak bind, lazy
bind and exports streams encoded as byte opcodes.

This PR recognizes each stream and its checked file bounds, and
detects LC_UNIXTHREAD for classification, while leaving actual guest
relocation and execution fail-closed. This is NOT decoding, applying,
import resolving, native ARM64 execution or UIKit rendering.

Next milestone is independent checked ULEB/SLEB bind/rebase decoder,
with dyld segment-index validation, pointer width/type and file/VM
bounds, fixture opcode tests including malformed opcode streams,
then transactional application to guest memory and unchanged original
UIKitCatalog Windows x86-64 Dynarmic entry retry.

Primary references:
https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOAnalyzer.cpp
https://github.com/llvm/llvm-project/blob/main/lld/MachO/SyntheticSections.cpp
https://lief.re/doc/stable/doxygen/classLIEF_1_1MachO_1_1DyldInfo.html
