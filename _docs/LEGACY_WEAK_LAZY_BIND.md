# Legacy weak and lazy binding metadata

The unchanged UIKitCatalog ARM64 binary is blocked by legacy linking, before
any guest instruction or window. The accepted regular/rebase scan observed
29 rebase sites and 4 eager bind sites on native Windows ARM64 in run
37915787958. This change adds read-only weak and lazy stream inspection to
both Windows probes; actual linking is still refused.

The shared decoder bounds streams to 16 MiB and sites/declarations to 65536.
All target locations require aligned file-backed read/write, non-executable
64-bit pointers. Checked ULEB/SLEB, symbol bounds, duplicate sites, ordinal
validation, and explicit terminators apply to every stream.

Weak binding uses the implicit special ordinal -3 (weak lookup), distinct
from the weak-import flag. Dylib ordinal instructions are forbidden in a
weak stream. NON_WEAK_DEFINITION flags produce separate symbol declarations,
including declarations without pointer sites; these must eventually be used
by a module coalescer, not discarded or treated as weak imports.

Lazy streams contain separately addressed records, each ending in DONE.
Each emitted site retains its record's byte offset. Trailing zero padding is
accepted. The supported grammar requires one bind per nonempty record and
explicit ordinal/symbol/segment in each record; state resets at boundaries
so independently invoked records cannot inherit another record's identity or
addend. SET_TYPE and address/repeated-bind arithmetic are rejected for lazy
streams, matching Apple's analyzer's lazy opcode vocabulary. No lazy dispatch
stub or eagerly applied lazy relocation is implemented here.

Threaded binds, text32 targets, symbols over 254 bytes, weak coalescing, module
resolution, actual relocation and LC_UNIXTHREAD execution remain unsupported.
Malformed or unsupported streams throw without returning a partial result.
Input files and guest memory are never modified by these inspectors.

## Validation

28/28 portable CTests and 25 Python tests passed locally with GCC 13.3.
Tests include separate lazy records, fresh addends, weak lookup versus import,
non-weak declarations, duplicate/missing identity/terminator/record binding,
forbidden/reserved opcodes and flags, and signed 64-bit SLEB endpoints.
The original-app workflow records site counts or the first precise decoder
failure on each host, preserving the unchanged executable's SHA256. A green
workflow with a recorded unsupported boundary is not original app startup.

## References

Semantic research only; no Apple implementation copied:
https://github.com/apple-oss-distributions/dyld/blob/fd8d0c4d52320ebf64db34f3cb280310d905c5ae/common/MachOAnalyzer.cpp
`forEachBind_OpcodesLazy` and `forEachBind_OpcodesWeak`.

Next: validated bind resolution and transactional relocation of an owned
legacy Mach-O fixture, then retry unchanged UIKitCatalog. Native Windows
ARM64 Darwin execution and original UIKit rendering/input remain unimplemented.
No API/gate inventory status is promoted by metadata decoding.
