# Bundle dependency discovery and atomic image sets

Original Wikipedia and UIKitCatalog main executables at PR #33 executed 212
ARM64 instructions and requested `dlsym(RTLD_DEFAULT,
"__previews_injection_jit_link_entrypoint")`. Their required debug dylibs were
not registered, so searching that incomplete namespace correctly stopped.
This milestone provides prerequisites for loading original bundle images; it
also inspects their real dependencies without pretending to stage them.

## Dependency discovery

`discover_dependencies` uses the same path expansion and graph traversal as
`plan_dependencies`, with a caller-owned reader of canonical relative paths.
It handles ordered `@rpath` candidates, loader/executable-relative paths,
exact `@loader_path` and `@executable_path` runpaths, inherited runpaths,
repeated dependencies and cached absent candidates. Its result owns image
metadata and records every absent strong, weak and system dependency with
its importing image. Absolute system install names are requirements; they
are never passed to a host filesystem reader or replaced with empty modules.
Malformed input and reader errors propagate. Cycles, bare install names,
nested runpaths and unsupported path forms fail explicitly. Discovery records
absolute guest runpaths as external prerequisites without calling a host reader.
It can inspect bundle candidates from other runpaths, but cannot certify
ordered search completion while external search directories are unknown.
The reported closure remains incomplete even if all bundle dependencies have
a candidate. Strict `plan_dependencies` still rejects absolute runpaths.

`plan_dependencies` retains its strict behavior: mandatory missing images and
system paths are errors. The returned dependency-first order is a metadata
ordering, not proof that constructors were invoked or modules became visible
in a running process. Discovery does not resolve symbols or seal dlsym scopes.

Bounds: 4096 modules, 64 dependency depth, 16384 edges, 512 inherited/current
runpaths, 65536 unique candidate reads and 4096 bytes per normalized path or
install name. Reader implementations must additionally bound file storage.

## Filesystem intake and identity

`anyios-bundle-probe <original.app> <relative-executable>` reads only discovered
bundle candidates. It accepts thin unencrypted ARM64 Mach-O metadata and
reports actual chained import counts and legacy eager/lazy/weak bind counts.
Its virtual `Bundle/` root prevents references outside the selected app;
symlink components, nonregular files, malformed content and inaccessible
paths stop intake. Files are bounded to 128 MiB each, 256 images and 256 MiB
total. It does not enumerate or load host system libraries.

`app_coverage.py <original.app> --verify-report <same-run-report.json>` verifies
the complete reported Mach-O set and each file's SHA-256/size, including debug
and embedded framework binaries, before dependency inspection. Original CI
keeps the metadata report separate from the actual main-entry execution
trace. `BUNDLE_STAGING=not-attempted` and
`BUNDLE_GUEST_EXECUTION=not-attempted` describe this intake exactly.

## Transactional staging primitive

`stage_linked_images` stages an already resolved set using one mapping journal
and the existing checked legacy/chained relocation mapper. One MH_EXECUTE,
at most 256 input images and 256 MiB of file bytes are accepted. All binding
patches, including signed addends, must point into an original readable
segment in this set, with a total cap of 65536 binding patches. Overlapping
readable segments, arbitrary existing trap pages, unmapped targets, rounded
page padding and overflow are rejected. After mapping, every target must be
readable before commit. Any failure rolls back the entire set while retaining
preexisting memory. Sources remain unchanged; executable pages remain RX and
data pages remain nonexecutable under the existing mapper.

This primitive accepts caller-resolved addresses; it does not resolve a
complete original application graph, fabricate framework implementations,
run constructors, publish a module registry or support runtime-module target
addresses outside the set. Existing `stage_owned_linked_pair` now uses it for
its real compiler-linked main/dylib fixture. That fixture's actual guest
cross-image call is tested independently on Windows x64 Dynarmic and the
existing native ARM64 owned runner. Owned fixture execution is not original
UIKit or native original-application acceptance.

## Validation and remaining work

Tests cover dependency/rpath ordering, exact anchors, missing requirements,
metadata ownership, cache behavior, cycles, malformed paths, limits and
reader failure propagation. A filesystem contract exercises a real bundle,
missing/malformed files, escape attempts, symlinks when the host permits
creation, directories and oversized files. Relocation tests cover cross-image
bindings/addends, W xor X, late-image rollback, preserved existing pages,
invalid targets and rejected sets. Native Windows/Linux ARM64 run these
portable contracts; the existing compiled cross-dylib fixture exercises the
new transaction on the actual execution backends.

Original workflows must supply fresh dependency evidence. Original apps are
still blocked at an incomplete loaded namespace; dependency metadata inspection
does not advance their 212-instruction entry trace. Next work is actual
multi-image symbol resolution and staging against implemented framework/data
exports, with required lifecycle/initializers and any observed weak-coalescing
requirements supported before publishing the scope. No API or runtime gate
inventory status changes in this milestone.

References (semantics researched; implementation independently authored):

- [Apple run-path dependent libraries](https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/RunpathDependentLibraries.html)
- [Apple dyld path resolution](https://github.com/apple-oss-distributions/dyld/blob/main/dyld/Loader.cpp)
- [Apple Xcode debug product layout](https://developer.apple.com/documentation/xcode/understanding-build-product-layout-changes)
