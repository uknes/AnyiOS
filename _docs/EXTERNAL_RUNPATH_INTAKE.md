# External guest runpath requirements

Original Wikipedia bundle discovery in PR #34 and original UIKitCatalog
metadata discovery with the modular cursor fix both stopped at
`unsupported dyld install-name prefix`. A metadata inspector should retain
absolute guest runpath requirements without reading host system directories.

Discovery now records absolute runpaths as external requirements with the
importing image's identity, and can inspect other real bundle candidates.
They are never supplied to the filesystem reader or resolved against host
Windows, Linux or macOS directories. Unknown external search directories may
change which file a real loader selects; therefore this partial metadata
traversal cannot certify ordered search completion. A closure with any
external runpath remains explicitly incomplete, including when every other
bundle dependency has an inspected candidate. The result is not a loaded
module namespace and cannot be used to authorize dlsym or app execution.

The strict `plan_dependencies` mapper prerequisite still refuses absolute
runpaths. Unsupported symbolic prefixes and malformed paths still stop;
diagnostics now include the actual unsupported prefix. Canonical paths and
length bounds use the existing path normalization, with at most 4096 external
runpath records. The metadata-only probe prints each image's original rpaths
and each external requirement, separately from unresolved strong/weak/system
library requirements and the original main guest execution trace.

Regression tests verify that real bundle candidates are read after an
external guest runpath, no absolute guest path reaches the host callback,
strict planning still refuses it, source metadata owns the requirement, and
length/count/escape/NUL/unknown-prefix failures remain explicit. The filesystem
contract includes an actual bundle with an absolute guest runpath and still
requires an incomplete result. Full original retries must reveal the concrete
prefix and next library/decoder requirements before any application advancement
is claimed. No original app files, completion inventory entries, module
staging/initializers or graphics behavior change.

ABI research references:

- [Apple public dyld Loader.cpp runpath traversal](https://github.com/apple-oss-distributions/dyld/blob/main/dyld/Loader.cpp)
- [Apple run-path dependent libraries](https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/RunpathDependentLibraries.html)
