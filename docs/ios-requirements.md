# Cross-app iOS compatibility requirements

Research snapshot: **2026-10-09**. The implementation checklist now contains 575 bounded gates across 53 subsystems, alongside 269 selected API exports. These are different units, not a combined percentage of iOS. The separate 405-entry Apple technology catalog is a discovery aid, not an SDK export inventory or a list of proven iOS dependencies.

## Acceptance rule

The goal is original unchanged application execution through shared platform behavior. Each app uses a subset of the platform. Broad compatibility requires implementing the union of those subsets, including dynamically discovered behavior, and testing it across unrelated apps. Neither Wikipedia nor UIKitCatalog defines the whole platform.

Each gate must be decomposed further when implementation reveals more contracts. A verified gate needs exact behavior, bounds, supported OS/ABI versions, Windows architecture, independent regression evidence and original-app evidence where applicable. A narrow fixture does not prove a complete framework. No new gate in this expansion is verified or partial.

## What the map includes

The [generated checklist](progress.md) lists every current runtime gate. The [Apple discovery catalog](apple-technologies.md) links every captured index entry, including technologies outside iOS that still need platform triage. The runtime expansion includes:

- CPU instruction correctness, PAC/BTI, atomics, page sizes, JIT invalidation, native traps and host isolation.
- Mach-O versions, legacy/chained fixups, dylib scope, initializers, dlopen, signatures, entitlements and resources.
- Mach IPC, XPC, kernel-observable semantics, clocks, signals, scheduling, memory pressure and sandboxing.
- libSystem, libc++, Blocks, unwinding, Objective-C reflection/ARC and Swift metadata/concurrency/ABI.
- Foundation/CoreFoundation, SwiftUI/Observation/Combine, UIKit, layout, text, localization and accessibility.
- Asset catalogs, localization, compression, file semantics, CoreData/SwiftData, shared containers and cloud storage.
- Metal including newer contracts, compiled shader formats, CoreImage, IOSurface, legacy GLES, game engines, AR and RealityKit.
- Audio, MIDI, codecs, media timelines, capture, photos, HLS, DRM-dependent media, broadcasting and streaming routes.
- TCP/UDP/TLS/QUIC, IPv6, network extensions, Wi-Fi, Bluetooth, WebKit and browser-engine isolation.
- Camera, motion, location, UWB, NFC, LiDAR/TrueDepth, Pencil, MFi, accessories, Matter and HomeKit.
- ML/AI runtimes, model formats/availability, OCR, speech, translation and local/cloud model services.
- Contacts, calendars, health, education, finance, weather, maps, age/parental permissions and other domain data.
- AppIntents/Siri, Spotlight, widgets, alarms, Live Activities, background execution and extension hosting.
- Calls, messaging, notifications, watches, continuity, SharePlay, vehicles and accessory forwarding.
- Commerce, identities, account services, attestation, trusted hardware, cryptography, privacy and device management.
- Third-party engines/runtime ABI, backend compatibility, undocumented requirements, obsolete services and cross-app evidence.

## Included external blockers

These dependencies must stay visible even if local implementation cannot satisfy them. **Pending is not synonymous with impossible.** Engineering feasibility below is a research assessment, not a verified failure of every app using the feature.

| Dependency | Requirement / limit to investigate | Evidence needed before support claims |
| --- | --- | --- |
| APNs, iCloud/CloudKit, Game Center | Account, identity, provider transport and backend acceptance | Authorized end-to-end original-app service test |
| StoreKit, receipts, subscriptions, Apple Arcade | Signed transactions, licensing and server verification | Legitimate transactions accepted by real service; no invented purchases |
| Apple Pay, Wallet credentials, Tap to Pay | Merchant trust, Secure Element, provisioning, device certification | Authorized hardware/service integration; substitute UI is not payment compatibility |
| App Attest, DeviceCheck, managed attestation | Device-bound keys and Apple/server trust | Real authorized verifier acceptance; no forged assertions |
| Secure Enclave, biometrics, Keychain protection | Hardware-backed key lifetime, authentication policy and enrollment | Document exact host mapping limitations; Windows authentication is not an Apple enclave |
| FairPlay/protected apps and media | Encryption, license and lawful original-input access | Authorized usable input; no decryption bypass or copied keys |
| Apple Intelligence and Private Cloud Compute | Framework API, models, licensing, supported devices and service access | Separately verify API behavior and authorized model/backend availability |
| UWB, LiDAR, TrueDepth, NFC, MFi, Pencil | Physical sensors/accessories and authenticated protocols | Supported adapter/device or genuine unavailable behavior |
| CarPlay/CarKey, HealthKit, HomeKit, finance and carrier services | Restricted entitlements, authorization, region and ecosystem state | Platform-specific authorized service and hardware test |
| Private APIs, anti-cheat and app servers | Undocumented behavior, integrity checks, backend/platform restrictions | Per-app reproducible lawful intake; no general support inferred |
| Removed or offline backend services | Runtime correctness cannot restore an unavailable third-party server | Clearly reported service availability and app fallback behavior |

Correct unavailable-feature errors can let an app use its existing fallback. They do not count as implementing the unavailable feature. A runtime cannot guarantee that an unchanged app has such a fallback.

## Discovery that is still required

No million-row list can honestly be called every requirement of every app without access to their binaries, assets, SDK versions and services. There is no claim that this research inspected every App Store application.

1. For each legally available SDK version, enumerate framework exports, Objective-C classes/selectors/properties, Swift symbols/metadata, constants, ABI layouts and availability. Public documentation alone is not a complete binary ABI specification.
2. Audit the 405 catalog entries for exact iOS/iPadOS availability, versions, deprecated status, tool/server-only scope and entitlement prerequisites. Non-iOS entries must not become iOS support claims.
3. Inspect lawful unchanged app bundles and every embedded image, resource, signature and entitlement. Record versions, CPU subtype, weak imports and dynamically requested symbols/selectors.
4. Measure original runtime execution, callbacks, graphics, input, persistence and network behavior on both Windows architectures. Exercise background/foreground, permissions, cancellation, error paths and concurrency.
5. Add previously unknown requirements, private contracts, third-party libraries and backend dependencies as they appear. Preserve unsupported outcomes instead of making success stubs.

## Primary research sources

- [Apple technology catalog](https://developer.apple.com/documentation/technologies), with the [structured public index](https://developer.apple.com/tutorials/data/documentation/technologies.json) captured on the research date. Only names and links are retained; no Apple implementation code is copied.
- [Technology Overviews](https://developer.apple.com/documentation/technologyoverviews): [data](https://developer.apple.com/documentation/technologyoverviews/data-management), [hardware/networking/sensors](https://developer.apple.com/documentation/technologyoverviews/hardware-networking-sensors), [audio/video](https://developer.apple.com/documentation/technologyoverviews/audio-and-video), [core experiences](https://developer.apple.com/documentation/technologyoverviews/core-experiences).
- [iOS what's new](https://developer.apple.com/ios/whats-new/), [WWDC26 iOS guide](https://developer.apple.com/wwdc26/guides/ios/), [iOS/iPadOS release notes](https://developer.apple.com/documentation/ios-ipados-release-notes). Retrieved material exposes iOS 27 topics and beta labels; exact shipping SDK/version availability remains unverified.
- [Entitlements](https://developer.apple.com/documentation/bundleresources/entitlements), [Foundation Models](https://developer.apple.com/documentation/foundationmodels), [Apple Platform Security](https://support.apple.com/guide/security/welcome/web).
- [Apple open-source distributions](https://github.com/apple-oss-distributions): ABI research must respect individual licenses and clean-room implementation boundaries.

## Reporting change

The 239-gate inventory was expanded to 575; the verified count remains 6 and partial count remains 11. The selected API inventory stays at 269 with 3 verified and 1 partial. The runtime percentage falls because the tracked scope grew, not because implementation regressed. Historical documents mentioning 239 describe older snapshots. The app-intake workflows now compare report counts with the actual source-owned inventories rather than hardcoded historical totals.
