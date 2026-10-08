# Application compatibility matrix (evidence required)

Status is **not tested** until an owned, reproducible build and gap-report artifact exists. No App Store compatibility claim follows from an owned fixture.

| App | License verified from file | Tier | Build / report status | Blocking APIs |
| --- | --- | --- | --- | --- |
| Candidate A (selection pending) | No | T1 | Not tested | Unknown |
| Candidate B (selection pending) | No | T1 | Not tested | Unknown |
| Candidate C (selection pending) | No | T2 | Not tested | Unknown |
| Candidate D (selection pending) | No | T3 | Not tested | Unknown |
| Candidate E (selection pending) | No | T3 | Not tested | Unknown |

## Tier gates

- **T1**: Foundation-only Objective-C CLI, class and selector resolution, messaging, memory ownership, exceptions.
- **T2**: one UIWindow, CoreGraphics drawing, deterministic touch input.
- **T3**: small UIKit app, lifecycle, hierarchy, controls and navigation.
- **T4**: Metal/OpenGL ES renderer with resource and shader behavior.

## Gap-report contract

Input: Mach-O executable or dylib and a versioned, evidence-backed implementation manifest. Output: machine-readable JSON plus Markdown summary. Extract undefined/imported symbols from the symbol table and bind/lazy-bind information; Objective-C class references and selector references from validated section metadata. Deduplicate with occurrence counts; report symbol, framework, tier, evidence, status (implemented / partial / not implemented / unknown), and blocker. **Static occurrence counts are not runtime call frequencies**; rank by distinct app prevalence first, then static occurrences. Include binary SHA-256, tool version, and manifest commit. Malformed binaries fail closed. Never infer implemented status solely from a matching exported name.

## Cross-app ranking

After five license-file-checked applications produce artifacts, aggregate missing API prevalence (number of apps), then tier, then static occurrences. Publish each build recipe, compiler flags, Mach-O SHA-256 and CI run. An app failing SDK-free compilation remains in the table as **build blocked**, not as a successful compatibility test.

## Prior-art boundaries

- microsoft/WinObjC: GitHub metadata reports MIT and archived, last pushed 2022-11-28. Check actual LICENSE and API coverage before deriving conclusions.
- touchHLE/touchHLE: GitHub metadata reports MPL-2.0 and active/unarchived, pushed 2026-10-05. Check actual LICENSE and UIKit implementation coverage before deriving conclusions.
- Design references only; **do not copy code**. Repo license metadata is not a substitute for checking license-file contents.

## Outstanding engineering gates

- Overlap-safe guest memcpy (memmove semantics) and chunked >64 KiB scans/copies, both overlap directions tested.
- errno, abort, write-backed stdio, single-thread pthread_once/mutex.
- Multi-module initializer ordering.
- Objective-C ABI metadata and dispatch notes.
- Hot-path ADR evaluating guest-side fast paths against SVC-per-call overhead.
