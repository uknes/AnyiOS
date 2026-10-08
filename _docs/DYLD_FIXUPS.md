# dyld chained fixups — scoped interoperability milestone

Research refreshed: 2026-10-08. Code is experimental and not a complete dynamic linker.

## Reference semantics

- [Apple dyld chained format definitions](https://github.com/apple-oss-distributions/dyld/blob/main/include/mach-o/fixup-chains.h): segment/page starts, `DYLD_CHAINED_PTR_64` (2), `DYLD_CHAINED_PTR_64_OFFSET` (6), import indexes and next strides.
- [Apple dyld MachOFile implementation](https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOFile.cpp): rebases and binds need different target interpretation and validation.
- [Mach-O nlist_64 ABI](https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/nlist.h): only externally visible, defined non-private section/absolute symbols are considered by our restricted resolver.

## Implemented subset

1. `macho::inspect` stores the validated `LC_DYLD_CHAINED_FIXUPS` payload range and imported symbol names.
2. `dyld::plan_chained_fixups` reads bounded page-start arrays, follows 4-byte-stride chains and decodes 64-bit rebase/bind pointer words. Only pointer formats 2 and 6, import descriptor format 1, and single-start pages are currently accepted.
3. `dyld::resolve_chained_import_targets` matches **positive, one-based** library ordinals to the image's ordered dependencies. An explicit, preloaded MH_DYLIB registry must provide exact install names and visible nlist_64 symbol values. The caller supplies a slide for each dylib.
4. `dyld::apply_chained_patches` validates all patch locations and overlaps before writing to a caller-owned byte buffer. The fixup engine does **not** patch executable guest memory directly.

## Supported tests

- Original synthetic chained rebase + bind at separate offsets, both pointer formats, patch values and no mutation of the original source.
- Failure cases: pointer format, multi-start page, missing library/symbol, invalid ordinal, reserved bits, truncated header, chain crossing a page, symbol visibility, duplicated symbols and overflowing locations.
- Cross-platform CTest targets: `macho-chained-fixups` and `macho-chained-imports`; sanitizer fuzz test continues to exercise the Mach-O metadata parser.

## Explicit gaps

- No Apple `arm64e` pointer authentication, pointer formats 1/7/9/12, authenticated binds, multi-start pages, import addend formats 2/3, or reexport lookup.
- nlist_64 matching is only a subset of real dyld exports; stripped dylibs often require validated export trie address lookup. No lazy bindings, global/flat lookup, weak-null resolution, in-memory final permissions, initializers, cross-module mapping or dyld shared cache.
- This code cannot launch a normal linked MH_EXECUTE, and is **not** a justification for removing the existing strict loader rejection.

## Acceptance for next milestone

Demonstrate a real, independently linked owned `MH_EXECUTE` + `MH_DYLIB` pair, extract/import actual symbol addresses, apply supported chain formats to a **staged** image, then execute code with a real dyld-dependent call on Windows x86-64 under permission-checked guest memory. Fail closed on any unsupported formats. Do not substitute hardcoded addresses for a claimed dynamic-linking demo.
