# Private HLE Foundation string object ownership (not an NSString implementation)

## Why this exists

Apple's Objective-C ABI includes an object-to-class relationship (`objc_object::isa`),
but a genuine `NSString` also requires a registered Foundation class, dispatch
semantics, reference counting/autorelease and Foundation/CoreFoundation behavior.
See [Apple objc-object documentation](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/ObjectiveC/Chapters/ocObjectsClasses.html)
and [Apple NSStringFromClass](https://developer.apple.com/documentation/foundation/nsstringfromclass%28_%3A%29?language=objc).

The previous milestone proved original `AppDelegate` class metadata and a
read-only, validated UTF-16 snapshot. This milestone constructs and destroys a
**private, diagnostic-only HLE object** with guest address identity and
guest-readable storage. It deliberately does **not** register an actual
`NSString` Class or return the private guest address through an imported
Objective-C or Foundation entry point.

## Scope and invariants

`GuestHleFoundationStringObjects`:
- Reserves a 16 KiB read/write guest object arena and a distinct 16 KiB
  read-only private class *marker* page. These pages are transactionally
  mapped, so constructor failure cannot strand the other mapping.
- Each object uses a 32-byte exact-address record (private class marker
  pointer; UTF-16 snapshot address; length; guard). The class marker is
  intentionally **not a valid objc_class**.
- Tracks one initial retain; explicit `retain()` and `release()` enforce
  the object's exact guest identity and immutable association with the
  corresponding read-only UTF-16 snapshot. The final release clears guest
  bytes and revokes access. Released addresses are never reused, preventing
  stale-reference/ABA aliasing.
- Exposes bounded `utf16_length`, `code_unit`, `equals` operations
  **only to the host-side diagnostic/prototype**. These are NOT dispatched
  `NSString` selectors.
- Rejects unallocated/interior/forged/stale references, corrupted isa,
  modified UTF-16 snapshot pointers, altered length or guard, malformed UTF-8,
  over-capacity requests, and guest arena/class mapping collisions.
- Limits total objects to 256 instances; UTF-16 storage remains separately
  capped at 32 distinct strings of 255 UTF-16 code units each.
- Does not implement concurrent guest execution, libobjc class registration,
  Foundation superclass methods, exception/destructor safety, copy semantics,
  real autorelease, ARC, CFString toll-free bridging, or full `objc_msgSend`.

## Original pinned ARM64 acceptance

The **unchanged, source-verifiable, SHA-256-checked** MIT Bitrise
Objective-C app must still enter through Dynarmic on Windows x64 and
report `AppDelegate`, exactly 11 UTF-16 code units, a private HLE owned
object + final release, then stop at the genuine
`FIRST_RUNTIME_BLOCKER=_NSStringFromClass`. No host "NSString" is
fabricated and **no private class-token or object address is returned to guest**.

A portable CTest checks Unicode surrogate units, distinct instance identity,
content equality, retain/release, zeroing and no address recycling, forged
pointers, corruption of all four guest header fields, capacity bounds,
and transactional rollback on partial mapping conflicts. The Windows
original-binary workflow explicitly executes this test and checks its
diagnostic markers.

## Next required milestone

Implement a verified Foundation `NSString` HLE class registry with correct
ObjC method dispatch, lifetime/autorelease ownership, and documented
`NSStringFromClass` return ABI. Only after original ARM64 guest code
can legally use a registered string class and dispatch supported methods
should we consider returning a guest-owned string object from
`_NSStringFromClass`. No compatibility gates are promoted based on
the current private class marker.
