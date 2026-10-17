# Guest UTF-16 Foundation class-name preflight (not NSString)

Apple public contracts: [NSStringFromClass](https://developer.apple.com/documentation/foundation/nsstringfromclass%28_%3A%29?language=objc) returns an actual `NSString *`, or `nil` for a nil class. [NSString](https://developer.apple.com/documentation/foundation/nsstring) lengths/indexes count UTF-16 code units, not UTF-8 bytes or Unicode scalar values.

This change prepares the *character storage*, not the Foundation object model.

## Implementation
- `GuestFoundationStringStorage` owns exactly 16 KiB of **read-only, non-executable guest memory** split into 32 slots of 512 bytes, each allowing at most 255 UTF-16 code units plus a terminating U+0000.
- Strict RFC 3629-style UTF-8 decoding (rejects overlong sequences, stray continuation bytes, truncated multibyte sequences, surrogate encodings and scalars above U+10FFFF). Supplementary code points encode as UTF-16 surrogate pairs and count as **two** code units.
- Explicit little-endian guest character encoding, independent of host `wchar_t` width and CPU endianness.
- Stable deduplication of identical source byte strings, immutable read-only guest data, no guest write/execute permissions, no acceptance of guest interior pointers, forged references, out-of-bounds code units or over-capacity allocations.
- `preflight_foundation_class_name` checks only compiler-local, validated Objective-C classes via the existing class registry. An unknown class fails closed, even if a host string looks plausible. Nil class is separately represented, not misreported as invalid.

## Exact original ARM64 execution expectation

The *general* Windows Dynarmic probe must run the **unchanged pinned MIT Bitrise Objective-C binary** and report:

```
SUPPORTED_NARROW_IMPORT=_objc_autoreleasePoolPush
SUPPORTED_NARROW_IMPORT=_objc_msgSend
METHOD=+class-local-identity
CLASS_NAME=AppDelegate
CLASS_NAME_UTF16_UNITS=11
FOUNDATION_STRING_PREFLIGHT=verified-guest-UTF16-not-NSString
FIRST_RUNTIME_BLOCKER=_NSStringFromClass
REASON=Foundation-NSString-object-unimplemented
WINDOW=not-created
```

These outputs are diagnostic proof, **not** successful NSString creation or UIKit execution. The guest UTF-16 address must never be sent to an arbitrary ARM64 guest as an Objective-C pointer; it has no `isa`, Objective-C methods, retain/release, autorelease or CoreFoundation toll-free bridge.

A nil class is the only case the general runtime can safely implement as `NSStringFromClass(nil) => nil` (x0 = 0). The unmodified pinned app uses a real AppDelegate class, and must remain blocked there until a correctly modeled Foundation object type exists.

## Regressions

`anyios-foundation-string-tests` checks ASCII, empty strings, Latin-1-range multibyte UTF-8, BMP U+20AC, a supplementary U+1F600 surrogate pair, UTF-16 lengths, boundary length 255, capacity and deduplication, mapping permissions, collision/alignment, and malformed UTF-8 rejection. The existing ObjC dispatch fixture now proves class-name preflight for an actual staged guest class metadata record, including nil and forged-class refusal. The original-pinned Windows CI workflow builds and explicitly runs both contracts.

Do not promote global compatibility percentages based on this substrate. Wikipedia and UIKitCatalog original-binary workflows must remain green, independent of this foundational work. No original app window or guest-rendered pixels are claimed.
