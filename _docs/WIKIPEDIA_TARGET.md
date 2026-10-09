# Wikipedia iOS — unchanged upstream ARM64 coverage intake

## Selection and permitted reproduction

Target: [Wikimedia Foundation Wikipedia iOS](https://github.com/wikimedia/wikipedia-ios), MIT license. Verified actual `LICENSE.txt` at immutable upstream source commit `d2b887fc8cefc42a4c06a1f9c82c6b1ac056d76c`. Use its original Xcode project, source code, resources and Swift/Objective-C modules unchanged. Never reimplement app screens in a host fake, strip DRM, obtain nonredistributable binaries, or claim a simulator screenshot is proof of Windows execution.

This is a high-complexity **stress app**, not a promise of all 269 exports or 239 gates. Its static imports can only identify symbols it references. Runtime gate evidence requires the original ARM64 program executing on each host and **real guest-originated windows and input**. Unused candidate APIs need independent sample apps and owned ABI fixtures. The inventories are provisional: 269 candidate exports in 19 groups, 239 implementation gates in 31 groups; they are not the full iOS API universe.

## Reproducible pipeline

`.github/workflows/wikipedia-original-ios.yml` clones the exact upstream SHA to a temporary directory on macOS and uses original `Wikipedia.xcodeproj` and `Wikipedia` scheme with generic iOS device and signing disabled. This compiles an **iPhoneOS ARM64** app, not an x86 simulator build. If upstream compilation fails due to unresolvable packages, Xcode/Swift changes or CI limitations, the job records `blocked-upstream-build` with log excerpt and does not upload a fake app or count it as a pass. The workflow is manually dispatchable and also runs on changes to itself, the target manifest, or its intake scripts.

If compilation succeeds, read-only `tools/app_coverage.py` examines Mach-O executables and embedded original frameworks without modifying them. `wikipedia-coverage.json` distinguishes candidate import presence, outside-inventory imports, unsupported parser formats, original SHA-256 and all 239 gates as **not app-tested**. No match in `tools/api_manifest.json` automatically demonstrates that an implementation works for Wikipedia.

First real dynamic test (NOT YET IMPLEMENTED): import and stage this very same artifact on Windows x64/Dynarmic and native Windows ARM64, verify original imports and Swift/ObjC/dyld lifecycle; stop on first unsupported guest runtime call; fix the runtime, never the original program. Do not claim a guest-originated visual until real application UIKit/scene objects, content, and translated interactive input render pixels in a Windows window, with CI screenshots and host architecture traces.

## Known likely blockers, not fabricated observations

Swift standard/runtime libraries and ABI metadata, framework dyld recursion, imported Objective-C class/metaclass registration, Foundation object behavior and Swift/ObjC bridging, UIApplicationMain/scene lifecycle, UIKit/CoreAnimation drawing, WebKit rendering/networking, threads and TLS, Windows ARM64 safe sandbox and SVC ABI. Actual priority must come from measured pinned `.app` imports and dynamic guest trace, not intuition.

Apple documentation: https://developer.apple.com/documentation/uikit/transitioning-to-the-uikit-scene-based-life-cycle .
Wikipedia source/build instructions: https://github.com/wikimedia/wikipedia-ios/blob/d2b887fc8cefc42a4c06a1f9c82c6b1ac056d76c/README.md .

## Acceptance levels

1. **Original source verified** — pinned SHA, MIT LICENSE, original project.
2. **Original binary built** — SHA-256 of exact untouched iPhoneOS Mach-O + bundle captured.
3. **Static import gaps** — reports include found candidate APIs, extra symbols, all 239 gates marked unexercised, parser limitations.
4. **Windows x64 / ARM64 stage** — exact binary mapped with no fabricated imports.
5. **Guest execution** — original ARM64 instructions run, crash/fault step and architecture recorded, no premature success.
6. **Actual app UI** — unmodified application creates/updates its own guest state and pixels displayed on Windows, keyboard/pointer input drives original guest callback; separately proven for x64 and native ARM64.
7. **Compatibility promotion** — only specifically proven ABI symbols/gates marked verified after regression across owning fixtures, Windows x64, Windows ARM64, and unchanged real apps.
