# AnyiOS compatibility coverage — counting and completeness rules

AnyiOS's first-party MIT-licensed generator recreates the public **behavior** of AnyPS5's two badge/two-panel function-tile treemap and change report. The original AnyPS5 generator is GPL-2.0-only; **none of its source is copied**.

## Two independent coverage metrics

| Metric | Catalog | Verified threshold |
|---|---|---|
| iOS API exports | [Representative API export inventory](../tools/api_inventory.json) + [current implementation evidence](../tools/api_manifest.json) | Export marked implemented with narrow scope/test proof; partial never counts as complete |
| Runtime compatibility gates | [Feature/capability inventory](../tools/compat_capabilities.json) | Gate explicitly verified with source/CI evidence; partial doesn't count |

**Neither percentage measures all APIs Apple has ever shipped, or the proportion of iOS games that will run.** The denominator includes only explicitly inventoried symbols and capability gates, and grows as real apps reveal new APIs. A status may fall when the inventory becomes more complete. Unknown does not imply impossible; incomplete Objective-C or a plausible no-op stub must not be counted implemented.

The two-panel [treemap](../docs/progress.svg) uses green for narrowly verified, amber for partial, and gray for pending/unverified. Hover a group for its counts, or use the [full accessible HTML report](../docs/progress.html), which lists **every individual tracked function and gate**.

## Required systems for a broad iOS compatibility layer

1. Mach-O thin/FAT/arm64e analysis, all load commands and protected-code detection.
2. dyld dependency graph, symbol resolution, rebases, fixups, ASLR, shared-cache compatibility and initializers.
3. ARM64 instruction translation on Windows x86-64, full ABI, vector registers, atomics, exceptions and traps.
4. Native ARM64 Windows execution with syscall traps, guest TLS, memory protection and real-game callbacks.
5. Sparse guest address space, stack, heap, memory permissions and sandbox boundaries.
6. Full Darwin libSystem and POSIX C files, memory, network, time, process and environment behavior.
7. Pthreads, dispatch queues, work groups, run loops, thread-local storage and synchronization.
8. Objective-C runtime classes/metaclasses, method dispatch, ARC, autorelease pools, exceptions, categories and selectors.
9. Swift ABI, reference counting, generics, protocols, value witnesses and concurrency.
10. CoreFoundation collections, strings, timers, bundles, CoreData and Foundation classes.
11. UIKit application and scene lifecycle, UIApplicationMain, delegates, storyboards, NIBs and UIWindow.
12. UIKit hierarchy, Auto Layout, controllers, navigation, gestures, text controls, accessibility and animation.
13. QuartzCore and CoreAnimation timing, layers, display links, transforms and masks.
14. CoreGraphics, CoreText and image/font decoding and compositing.
15. Metal render/compute pipelines, shaders, buffers, textures, command queues and display surface.
16. Legacy OpenGL ES API translation and framebuffer extensions.
17. SpriteKit and SceneKit scenes, physics, particles, action schedulers and assets.
18. Unity IL2CPP/Mono, Unreal, cocos2d and other engine-specific native dependencies.
19. CoreAudio, AVFoundation, audio/video codecs, clocks and playback devices.
20. Win32 mouse/keyboard mapped to multitouch, controllers, accelerometer, gyro and haptics.
21. URLSession, CFNetwork, sockets, TLS, DNS and networking permissions.
22. Security.framework, certificates, Keychain, CommonCrypto, file protection and biometrics.
23. Persistent save games, SQLite, Core Data, CloudKit, documents and caches.
24. Game Center/GameKit, leaderboards, in-app purchases/StoreKit and platform entitlements.
25. WebKit/JSC, browser sandbox and embedded web content.
26. Notifications, camera, location, widgets, contacts and optional iOS capabilities.
27. Mach IPC, XPC, virtual OS/environment/version reports, locale, device and accessibility APIs.
28. Guest-created UI rendering to Windows Direct3D/Win32, input, resizing, frame pacing and focus.
29. Original binary intake, import coverage reporting, fuzzing, deterministic tests and game compatibility evidence.
30. Windows x86-64 and ARM64 release packaging, performance testing, update/install infrastructure and licenses.

Each numbered system is subdivided into numerous individually scored items. These are planning requirements; **a list of 239 gates is not exhaustive**. In particular protected retail App Store apps may require legitimate entitlements and distribution rights; bypassing DRM is not a promised compatibility feature.

## Rebuild and audit

Run the following from the repository root:

    python3 tools/progress.py docs
    python3 tools/progress.py docs --check
    python3 -m unittest discover -s tests -p test_progress.py -v
    python3 -m unittest discover -s tests -p test_unchanged_ipa_probe.py -v

Generated deliverables: `docs/badge-apis.svg`, `docs/badge-runtime.svg`, `docs/progress.svg`, `docs/progress.json`, and `docs/progress.html`. CI tests the counts and XML, compares changes on new work, and rejects stale committed assets on main.

Compare before/after progress JSON using:

    python3 tools/progress.py --compare before/progress.json after/progress.json

## Advanced unmodified iOS game: Sneaky Sasquatch

[Sneaky Sasquatch](https://apps.apple.com/gb/app/sneaky-sasquatch/id1098342019), a recognized open-world Apple Arcade game, is the chosen iOS-exclusive compatibility research target. Its commercial original executable has **not** been obtained, decrypted, or launched. It remains **blocked on an authorized, unprotected original ARM64 iOS binary**, and substantial runtime support.

Run `python3 tools/unchanged_ipa_probe.py --target sneaky-sasquatch` to get an explicit blocked-no-binary report. If the user legitimately holds a usable unprotected original .ipa or .app, run `python3 tools/unchanged_ipa_probe.py /path/to/OriginalGame.ipa --target sneaky-sasquatch` to inspect SHA-256, encrypted-code indicators, dependencies and imports. This **never changes the original file**, extracts no assets, bypasses no protections, and does not claim game execution.

A real successful port would require unchanged original game logic, assets and executable, original guest instructions, original game-created pixels and functional input. An AnyiOS demo game or fake window is **not** a port.

Sources: [AnyPS5 code-generation reference](https://github.com/boykopovar/AnyPS5/blob/main/tools/progress.py) ([counting rules](https://github.com/boykopovar/AnyPS5/blob/main/docs/dev/PROGRESS.md)); [Apple's August 2026 Sneaky Sasquatch announcement](https://www.apple.com/newsroom/2026/08/exciting-updates-for-sneaky-sasquatch-come-to-apple-arcade/).

## 2026-10-08 — Objective-C local class metadata registry

Source PR [#11](https://github.com/uknes/AnyiOS/pull/11) introduces a strict lookup table backed by actual ARM64 Clang Objective-C local `__objc_classlist` and `class_ro_t` records, validates class names and local superclass pointers, and rejects unknown imported framework superclass references rather than inventing them. Portable Windows x64 class-registry CTest and native Windows ARM64 class-registry CTest are green on [run 37850732319](https://github.com/uknes/AnyiOS/actions/runs/37850732319); the pinned original MIT Bitrise Mach-O Dynarmic launch-boundary job must also pass before PR merge. This is **partial** progress for the single gate `Class registry, metaclasses and inheritance`; neither metaclasses nor general Objective-C class registration are implemented. `_objc_getClass` is only recognized for registered local names and is not counted as a complete API export.

The library API implemented count remains 3, partial 1 of 269. Runtime verified remains 6 of 239; partial rises from 10 to 11. The README badges count **verified** only, so they do not imply increased full compatibility.
