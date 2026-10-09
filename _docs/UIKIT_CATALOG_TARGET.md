# Original Appium UIKitCatalog — graphical acceptance target

Original app: https://github.com/appium/ios-uicatalog at immutable SHA
`5855a3e05ec047c796514c93ce4dd22490af1905`. This upstream
source is Apache-2.0-licensed, with a checked-in
`UIKitCatalog/UIKitCatalog.xcodeproj` and shared `UIKitCatalog` scheme.
Appium uses it as a genuine iOS user-interaction test application.
It is **not** the original Wikipedia production stress app; both remain
separate unchanged test inputs.

## CI target

`.github/workflows/uikit-catalog-original-ios.yml` fetches that exact
source revision on macOS, verifies its Apache license and clean tracked
working tree, then attempts the untouched shared-scheme Xcode
`-sdk iphoneos -destination generic/platform=iOS` **ARM64 device**
build with signing disabled. It records xcodebuild logs on error instead
of reporting a dummy success. A successful original .app is hashed
and statically scanned with the same 269 candidate API and 239
unexercised-gate accounting as Wikipedia. Static imports do **not**
prove guest execution.

## Native graphical acceptance (NOT YET ACHIEVED)

1. The *original* ARM64 app creates its UIKit application/window and
   keeps a guest-backed lifecycle/event loop alive on Windows x86-64
   Dynarmic and Windows ARM64-native.
2. A guest-produced layout genuinely renders Appium's sample controls:
   e.g. UIButton, UISlider, UISwitch, UITextField, UIAlertController
   (not host HTML, a hard-coded screenshot, or a mocked Win32 view).
3. Windows pointer/touch/key events call back into the unchanged guest
   UIKit target/action path. Screenshot/recording and guest event
   checks must corroborate the displayed state and visual updates on
   **both** architectures.
4. The exact original Mach-O SHA-256 is checked on the execution host.
   There must be no source/project edits, injected UIKit implementation
   inside the app, DRM bypass or proprietary framework redistribution.
5. API/gate completion counts change only after independent ABI
   assertions pass, not just because an import exists.

The app's original UI remains **not running on Windows**. This initial
PR is intake/build provenance and coverage only, while Wikipedia's
first real imported-call blockers continue to be fixed separately.

Source: https://github.com/appium/ios-uicatalog ; Apple's control reference:
https://developer.apple.com/documentation/uikit/uikit-catalog-creating-and-customizing-views-and-controls .

## Source-matched original UIKitCatalog Windows staging and first instruction trial

A stacked PR now adds Windows x86-64 Dynarmic guest entry with same-run Xcode build SHA-256 verification. It must either execute original UIKitCatalog ARM64 instructions and report the first missing import, or classify a loader failure *before execution*. Windows ARM64 does only native Mach-O metadata staging. Neither result proves native Windows ARM64 guest execution, UIKit pixels, input or original app UI; these remain explicit acceptance gates.

## First measured native ARM64-host loader blocker from original UIKitCatalog

Original pinned source-built UIKitCatalog app (Xcode build verified) reached a native Windows ARM64 Mach-O stage in run [37870715261](https://github.com/uknes/AnyiOS/actions/runs/37870715261). It printed `STATIC_IMPORTS=0`, `STAGING=blocked`, `FIRST_LOADER_BLOCKER=unsupported linked image legacy dyld/thread state`. This is an actual original app Mach-O format **unsupported by the current chained-fixups-only linked loader**, not proof of UIKit failure after process startup. Native staging returned expected status 3; PowerShell exited nonzero because the last external command's nonzero code was propagated even after reporting the blocker. The workflow now explicitly exits zero ONLY after verifying expected and recorded `STAGING=blocked` / `STAGING=passed-metadata-only` with status 0 or 3; all abnormal statuses remain errors. Acceptance does not claim guest execution or UI. Next real loader subtask: recognize original legacy LC_DYLD_INFO / LC_DYLD_INFO_ONLY and LC_UNIXTHREAD formats separately, implement bounded bind/rebase with format-specific tests, then retry the UNMODIFIED original executable.
