# Application compatibility matrix (evidence required)

Status is **not tested** until an owned, reproducible build and gap-report artifact exists. No App Store compatibility claim follows from an owned fixture.

| App | License verified from file | Tier | Build / report status | Blocking APIs |
| --- | --- | --- | --- | --- |
| Bitrise sample-apps-ios-simple-objc @ `91fef6f` | **Yes**, actual MIT LICENSE checked | T3 | Three upstream .m files SDK-free compiled/linked via metadata-only placeholder; Windows x64 staging passed in CI 37797367626; not runnable | 29 symbols unresolved, including ObjC messaging/allocation, UIKit launch, Core Data; 24 encoded selector refs unresolved |
| [Sneaky Sasquatch](https://apps.apple.com/gb/app/sneaky-sasquatch/id1098342019) (RAC7, Apple Arcade) | Proprietary; original binary not supplied, no redistribution or modification permitted | T4+ | **BLOCKED**: no authorized unprotected original binary; no code executed, no window; see [target](../compatibility/targets/sneaky-sasquatch.json) and [issue #9](https://github.com/uknes/AnyiOS/issues/9) | Exact import list cannot be determined until lawful intact IPA available; UIKit, engine graphics, audio/input, entitlements unverified |
| [Danqing 2048 iOS](https://github.com/danqing/2048) @ `6f89eab8` | **Yes**, MIT LICENSE file checked | T4 (SpriteKit) | Original upstream main.m and M2AppDelegate.m compiled as ARM64, Windows x64 entry executes until unsupported NSStringFromClass in [PR #7 CI 37823115275](https://github.com/uknes/AnyiOS/actions/runs/37823115275); complete game and storyboard not run | Full Objective-C, UIKit and SpriteKit scene/graphics |
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

## First external build and staging protocol

- Upstream: `https://github.com/bitrise-io/sample-apps-ios-simple-objc`, immutable commit `91fef6f5a096220669934793a9256128bc73f25b`. Actual upstream `LICENSE` text confirmed as MIT, copyright 2014 Bitrise. External code stays fetched from the pinned upstream repository; no code from prior-art emulators is copied.
- `tests/build_bitrise_probe.py` attempts to compile **original** `main.m`, `AppDelegate.m`, `ViewController.m`, using **AnyiOS-authored declaration-only** Foundation/UIKit/CoreData headers. Apple SDK and Apple framework implementations are absent.
- Linking uses an automatically generated TAPI **metadata-only placeholder** that declares unresolved original object imports. This only produces a binary for gap analysis and staging; it is **not an implementation**. A successful link/stage **does not mean this app can run**.
- Windows `anyios-app-stage-probe` inspects and attempts guest-image section/fixup staging against inert placeholder addresses. It **never executes** external ObjC instructions or considers the placeholder dylib a working runtime. First stage error and full unimplemented import list are preserved in CI artifacts. App starts/renders/touches: **not implemented**.
- CI workflow `.github/workflows/compatibility.yml`: owned gap test, upstream SDK-free compile attempt, static import report, Windows x64 staging classification. Trust the exact run and step only after green CI; never promote a "blocked" app build to "app runs".

## First measured external app (CI 37797367626)

- `ios-gap-linux`: original MIT-licensed 3 Objective-C translation units compiled and linked into ARM64 Mach-O using project-authored declaration-only headers and metadata-only exports; no Apple SDK or UIKit/ObjC runtime. Binary SHA-256: `dbea1e0888f997c6a04ac2d69b69a196a88dd056fb0baf68edf0ab808745bf7d`.
- Static analyzer: **29 distinct imported symbols**, including 9 external ObjC classes, 40 candidate method-name strings, and 24 unresolved encoded selector pointers. None of the 29 is satisfied by the currently CI-verified narrow guest runtime; imports are not actual runtime call counts. Artifact `anyios-compatibility-probe`.
- `ios-gap-windows-x64`: exact Linux-produced binary inspection and sections/fixups mapped: `STAGING=passed-metadata-only`, guest entry 81920. **No guest code executed** in this step. Artifact `anyios-windows-compatibility-probe`.
- Windows x64 Dynarmic **entry-only** execution added, verification pending; must stop on the first unsupported guest import without pretending ObjC initializer or UIKit startup occurred.
- Evidence gates: `compiled` and `staged` verified; `launched`, `window`, `touch`, `interactive` **not implemented**.

## Proprietary original-binary game gate (2026-10-08)

The selected advanced iOS-only mobile game is RAC7 **Sneaky Sasquatch**. Apple's [August 2026 announcement](https://www.apple.com/newsroom/2026/08/exciting-updates-for-sneaky-sasquatch-come-to-apple-arcade/) confirms Apple Arcade distribution. No executable or rights to redistribute it have been provided. `tools/unchanged_ipa_probe.py` can read and hash a **legitimately obtained original unprotected** `.ipa`/`.app` without changing, patching, decrypting or resigning it. Its recorded status remains **blocked-no-original-app-binary / game-not-executed** until genuine input exists. Such a scan would still only be static import discovery, not a Windows port. A real port requires actual unchanged ARM64 game code, original graphics and input working on Windows x86-64 and Windows ARM64.
