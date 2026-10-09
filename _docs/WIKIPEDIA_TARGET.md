# Wikipedia iOS — unchanged upstream ARM64 coverage intake

## Selection and permitted reproduction

Target: [Wikimedia Foundation Wikipedia iOS](https://github.com/wikimedia/wikipedia-ios), MIT license. Verified actual `LICENSE.txt` at immutable upstream source commit `d2b887fc8cefc42a4c06a1f9c82c6b1ac056d76c`. Use its original Xcode project, source code, resources and Swift/Objective-C modules unchanged. Never reimplement app screens in a host fake, strip DRM, obtain nonredistributable binaries, or claim a simulator screenshot is proof of Windows execution.

This is a high-complexity **stress app** for the general runtime. Its static imports only identify symbols it references. Runtime gate evidence requires the original ARM64 program executing on each host and **real guest-originated windows and input**. Unused candidate APIs need independent sample apps and owned ABI fixtures. The [current API inventory](../tools/api_inventory.json) and [expanded runtime checklist](../tools/compat_capabilities.json) remain provisional; they are not the full iOS API universe or an app-compatibility percentage.

## Reproducible pipeline

`.github/workflows/wikipedia-original-ios.yml` clones the exact upstream SHA to a temporary directory on macOS and uses original `Wikipedia.xcodeproj` and `Wikipedia` scheme with generic iOS device and signing disabled. This compiles an **iPhoneOS ARM64** app, not an x86 simulator build. If upstream compilation fails due to unresolvable packages, Xcode/Swift changes or CI limitations, the job records `blocked-upstream-build` with log excerpt and does not upload a fake app or count it as a pass. The workflow is manually dispatchable and also runs on changes to itself, the target manifest, or its intake scripts.

If compilation succeeds, read-only `tools/app_coverage.py` examines Mach-O executables and embedded original frameworks without modifying them. `wikipedia-coverage.json` distinguishes candidate import presence, outside-inventory imports, unsupported parser formats, original SHA-256 and every inventoried gate as **not app-tested**. Counts derive from the current source inventories. No match in `tools/api_manifest.json` automatically demonstrates that an implementation works for Wikipedia.

The workflow executes this exact original main executable through Windows x64 Dynarmic and repeats that x64 host program under Windows ARM64 x64 emulation. Native Windows ARM64 runs read-only bundle inspection and main-image staging; it does not execute the original application. The latest completed original run, [37975403570](https://github.com/uknes/AnyiOS/actions/runs/37975403570), stopped after 212 guest instructions at `dlsym`, because the debug dylib was not loaded. Original app code, Swift/ObjC lifecycle and UIKit initialization have not run. Do not claim a guest-originated visual until actual application scene objects and translated interactive input render pixels in a Windows window, with CI screenshots and architecture traces.

## Known likely blockers, not fabricated observations

Swift standard/runtime libraries and ABI metadata, framework dyld recursion, imported Objective-C class/metaclass registration, Foundation object behavior and Swift/ObjC bridging, UIApplicationMain/scene lifecycle, UIKit/CoreAnimation drawing, WebKit rendering/networking, threads and TLS, Windows ARM64 safe sandbox and SVC ABI. Actual priority must come from measured pinned `.app` imports and dynamic guest trace, not intuition.

Apple documentation: https://developer.apple.com/documentation/uikit/transitioning-to-the-uikit-scene-based-life-cycle .
Wikipedia source/build instructions: https://github.com/wikimedia/wikipedia-ios/blob/d2b887fc8cefc42a4c06a1f9c82c6b1ac056d76c/README.md .

## Acceptance levels

1. **Original source verified** — pinned SHA, MIT LICENSE, original project.
2. **Original binary built** — SHA-256 of exact untouched iPhoneOS Mach-O + bundle captured.
3. **Static import gaps** — reports include found candidate APIs, extra symbols, every current gate marked unexercised, parser limitations.
4. **Windows x64 / ARM64 stage** — exact binary mapped with no fabricated imports.
5. **Guest execution** — original ARM64 instructions run, crash/fault step and architecture recorded, no premature success.
6. **Actual app UI** — unmodified application creates/updates its own guest state and pixels displayed on Windows, keyboard/pointer input drives original guest callback; separately proven for x64 and native ARM64.
7. **Compatibility promotion** — only specifically proven ABI symbols/gates marked verified after regression across owning fixtures, Windows x64, Windows ARM64, and unchanged real apps.

## First actual upstream build blocker and correction

The initial macOS 15 run [37864921845](https://github.com/uknes/AnyiOS/actions/runs/37864921845) captured original Xcode exit 74: `package 'wmfcomponents' is using Swift tools version 6.2.0 but the installed version is 6.1.0`. This is a **host toolchain mismatch**, not an AnyiOS guest ABI bug. The workflow now uses the officially supported GitHub Actions `macos-26` ARM64 runner with default Xcode 26 / Swift 6.2 or later; verify it in the new exact-head CI logs. Upstream source stays unchanged.

Official GitHub runner availability: https://github.blog/changelog/2026-02-26-macos-26-is-now-generally-available-for-github-hosted-runners/ . Apple Xcode 26 includes Swift 6.2: https://developer.apple.com/documentation/xcode-release-notes/xcode-26-release-notes .

## Second real upstream build blocker and fix

The first Xcode 26 / Swift 6.3 run [37865064944](https://github.com/uknes/AnyiOS/actions/runs/37865064944) passed Swift package-version resolution but stopped with Xcode 65: the upstream Xcode project expects `Configurations/OpenSourceDebug.xcconfig`, which is generated and ignored by Wikimedia's setup. Inspected upstream `scripts/setup_bundle_id`: its `ci` argument generates only that ignored build configuration using upstream logic and avoids an interactive developer-team prompt. Workflow now runs `(cd upstream && bash scripts/setup_bundle_id ci)` and explicitly verifies no *tracked* upstream source/project files changed before building. This is not a modified original iOS app.

## Source-matched Windows x86-64 original ARM64 instruction regression

With the general entry probe from stacked PR #19, the same CI workflow now adds a Windows x86-64 Dynarmic job that downloads *its own original Xcode build artifact*, validates Wikipedia.app/Wikipedia SHA-256 against the static-intake report, and executes the exact original Apple ARM64 instructions until an authentic guest loader or unresolved ABI blocker. This is not merely static staging. If the loader cannot map the real app format, the report is explicitly `blocked-original-wikipedia-MachO-loader-before-execution`; do not call that execution. If ARM64 code runs, require nonzero guest step count and actual first failure. An unsupported event is not an implemented API. Host Windows ARM64 is separately native *staging only* until a secure native Darwin exception/syscall/ABI sandbox exists. No fake guest UI, UIWindow or input is generated.

## Windows ARM64 host through x64-emulated Dynarmic (not native ARM64 Apple execution)

A separate, stacked workflow job reuses the **exact same CI-built original Wikipedia iOS ARM64 .app** and the **same x64 AnyiOS Dynarmic host executable** as the Windows x86-64 job. It executes the x64 host via documented Windows 11 ARM64 x64 app emulation, while Dynarmic runs the actual original ARM64 guest code. It checks the original binary SHA and requires a true guest-step trace and os_log_create advancement on the ARM64 Windows machine. This is **not** a native Apple ARM64 process or a Windows ARM64 JIT and does **not** constitute real UIKit windows or touch. The user's native ARM64 execution goal remains separate and unverified until proper protected Darwin SVC/TLS/ABI and thread handling are implemented.

## Large original symbol tables

The exact-head original run above exposed `symbol count exceeds safety limit` during bundle discovery. The original debug dylib has 382,495 nlist entries and WMF.framework has 606,764; their copied symbol names occupy 22,008,418 and 48,490,638 bytes respectively. These include real local/debug entries and are not that many imported APIs.

The shared parser now retains all entries under explicit one-million-entry / 64 MiB copied-name budgets per image, with unchanged 16 KiB individual-name bounds and strict file/string-table validation. Local inspection accepted all 15 hash-verified original Mach-O files. Dependency discovery inspected four reachable images and still reported an incomplete closure with missing Apple/Swift libraries and external guest runpaths. Both native Windows host architectures now require successful original large-table bundle inspection in CI; accepting a parser diagnostic no longer passes that regression. See [policy and tests](LARGE_SYMBOL_TABLES.md). Native host metadata parsing is distinct from native guest execution.
