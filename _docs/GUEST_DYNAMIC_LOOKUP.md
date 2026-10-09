# Bounded guest dynamic symbol lookup

The original Wikipedia and UIKitCatalog main executables stopped at instruction
212 in memory head 84aac9aed799e6f01059604c11d6842eb4c113b3, runs 37932339277
and 37932339285, requesting
`dlsym(RTLD_DEFAULT, "__previews_injection_jit_link_entrypoint")`.
These are Xcode Debug stub-executor instructions. Original app code resides in
the unchanged bundle's debug dylib; it has not initialized UIKit or a window.

## Implemented contract

GuestModuleRegistry snapshots already mapped main/dylib export metadata in load
order. Modern and legacy export tries are decoded by the same bounded parser.
Where no trie exists, a declared LC_SYMTAB supplies defined external nlist
symbols; private/local/debug/undefined symbols are excluded. Missing export
metadata remains unsupported. The shared definition predicate also serves the
existing chained import resolver.

RTLD_DEFAULT (-2) searches global modules in registration/load order;
RTLD_MAIN_ONLY (-5) searches the main executable. The source-level symbol name
is prefixed once with `_`, not normalized or stripped. Both function and data
addresses are guest addresses validated against their image and live readable
mapping. No host dlsym/GetProcAddress, guest-to-host pointer casts, or hardcoded
Previews-symbol result exists. Returned guest functions execute through the
normal guest CPU path. Absolute/TLS exports, reexports and resolver callbacks
stop explicitly when selected, as do caller-relative and arbitrary dlopen
handles. Runtime module exports require real mapped guest RX thunks.

Global searches require a sealed registry: every non-weak declared dependency
must have a registered module identity. Sealing verifies closure, not performs
loading. Registration is immutable afterward. The implementation snapshots
names/values rather than retaining pointers into caller-owned Mach-O metadata.
Module/export/name budgets bound allocations. Missing symbols return NULL only
after searching a supported, complete scope; unsupported/incomplete scopes
stop without inventing a result.

GuestDlState reads a bounded guest NUL-terminated name (up to 1024 bytes) and
owns separate non-executable error pages for explicit guest thread IDs. A
completed dlsym clears that thread's previous error; a missing symbol records a
real bounded diagnostic. dlerror returns its guest pointer once and clears the
indication, preserving the string until a subsequent lookup overwrites it.
Other threads retain their own errors. Invalid pointers, occupied buffers,
limits and unsupported searches fail closed. No host TLS is exposed.

## Diagnostic original-app integration

The general probe's libSystem compatibility module exports only its six actual
implemented ABI thunks: os_log_create, os_log_type_enabled, getenv, memcpy,
dlsym and dlerror. This is AnyiOS's narrow implementation namespace, not Apple's
full libSystem export inventory. Unresolved original import traps are excluded
from this export namespace and still stop if called. Extra runtime thunks are
separate from original import bindings; the file is never changed.

At the first lookup, the probe registers the mapped original main image and
this concrete runtime module, then verifies the declared dependency closure.
If any other required image is missing, it stops with that identity. A genuine
negative lookup in the supported namespace records dlerror; no symbol-specific
startup bypass is used. Bundled debug dylibs are not falsely marked loaded.
The original workflows must determine the resulting next actual blocker.

This registry does not implement dlopen, path/rpath alias resolution, image
unloading, dynamic interposition, initializer ordering, Swift/Objective-C
registration, or concurrent host calls. Guest thread selection is explicit and
sequential; this is not a scheduler. Error mappings live with the guest memory
process. Native Windows ARM64 import/SVC execution remains unsupported.

## Verification

Portable contract tests cover load order, main-only and local visibility,
function/data mapping, metadata lifetime, failed registration/sealing,
malformed names/addresses, unsupported export kinds/handles, thread isolation,
one-shot errors, buffer ownership and limits. Native Windows and Linux ARM64
run the same host contracts without x64 emulation.

The project-owned SDK-free Clang DynamicLookupApp dynamically finds an original
function in libRuntimeWidget.dylib, calls its ARM64 instructions, checks true
missing-symbol/error semantics, finds and calls a main-only function, and reads
exported guest data. Expected result: 58. Windows x64 Dynarmic CI must execute
it, rather than count static imports as success. This owned fixture is not
original third-party app startup or UIKit compatibility. No API/gate inventory
promotion follows from this bounded subset.

## Public ABI references

- https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/dlsym.3.html
- https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/dlerror.3.html
- https://github.com/apple-oss-distributions/dyld/blob/main/dyld/DyldAPIs.cpp
- https://github.com/apple-oss-distributions/dyld/blob/main/dyld/Loader.cpp
- https://developer.apple.com/documentation/xcode/understanding-build-product-layout-changes

Implementation is independently written; Apple source was read for ABI and
scope behavior, not copied into the compatibility layer.
