# Transactional legacy ARM64 pointer relocation

An explicit `LinkedImageOptions::allow_legacy_fixups` opt-in enables bounded
legacy pointer relocation through the existing staged-file copy and guest
mapping journal. Default callers continue to refuse legacy linking.

`plan_legacy_fixups` requires a thin unencrypted legacy-only image. Targets
must be supplied in eager-site then lazy-site order, with an exact count.
It applies checked signed addends, rejects null/unresolved targets and
cross-stream duplicate binds, and emits original pointer values for rebases.
The linked mapper then validates rebase targets against actual image segments
and translates original VM addresses to the guest base without unsigned-slide
wraparound. Source file bytes remain unchanged. Final guest pages use their
validated permissions, never simultaneous write+execute.

Lazy pointers are prebound eagerly. Where a lazy pointer was also rebased to
its stub helper, its resolved binding supersedes that rebase. A binder may not
silently overwrite another eager/lazy binder. Weak coalescing, threaded binds,
arm64e and LC_UNIXTHREAD entry remain refused. This is not a general dyld
module graph, framework resolver, coalescer or native Darwin executor.

## Original application probes

The non-executing stage probe uses explicitly labeled, unresolved metadata
placeholders. The Dynarmic research probe uses guest RX diagnostic SVC traps
for each original bind site; those are **unresolved imports**, not functioning
UIKit/Foundation implementations. The unchanged original entry instructions
may run until the first such unsupported import. Dependency dylibs and
initializers are not executed; no graphical startup can be claimed. Data
imports are not implemented by these traps. Callers of the loader are
responsible for guest target provenance; no host pointer is dereferenced or
cast from guest data by this loader.

The original UIKitCatalog workflow checks the same-run executable SHA256,
then records actual staging or entry failure on Windows. A green workflow
which records a loader boundary is not evidence of original guest execution.

## Validation

28 CTests and 25 Python tests passed locally. A project-owned synthetic
legacy Mach-O verifies rebasing, eager binding, lazy prebinding overriding
helper rebases, original input preservation and guest page protections.
Negative tests cover target counts/null, invalid rebase targets, duplicate
bindings, unimplemented weak coalescing, and late mapping failure preserving
an occupied guest page while rolling back earlier code pages. General entry
probe passed GCC syntax/warning checks. Cross-platform and exact original-app
CI remain required before acceptance. No inventory completion is promoted.

Semantic reference only (no source copied):
https://github.com/apple-oss-distributions/dyld/blob/fd8d0c4d52320ebf64db34f3cb280310d905c5ae/common/MachOAnalyzer.cpp
and public LC_DYLD_INFO descriptions of eager binding of lazy pointers:
https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/loader.h
