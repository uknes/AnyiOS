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
