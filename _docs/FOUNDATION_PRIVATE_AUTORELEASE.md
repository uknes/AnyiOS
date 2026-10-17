# Private HLE Foundation autorelease scopes

## Research basis and goal

Objective-C autorelease pools are LIFO scopes. A pool pop must release the
references queued since its matching push. The GNUstep libobjc2 runtime
provides an open-source implementation and documents `objc_autorelease`,
`objc_autoreleasePoolPush`, and `objc_autoreleasePoolPop`:
- https://github.com/gnustep/libobjc2/blob/master/objc/objc-arc.h
- https://github.com/gnustep/libobjc2/blob/master/arc.mm

AnyiOS does **not** use GNUstep's Objective-C ABI as a substitute for Apple's
or execute proprietary Apple runtime binaries. This milestone validates the
*ownership mechanics* for a **private HLE Foundation-string prototype only**.

## Exact scope

`GuestFoundationAutoreleaseScopes` composes the already bounded,
guest-memory-backed `GuestAutoreleasePools` token ABI with
`GuestHleFoundationStringObjects` private HLE retain/release.

- Uses unique, non-recycled pool tokens, enforces per-thread LIFO frames,
  and refuses cross-thread/forged/out-of-order/replayed pool tokens.
- `enqueue` schedules **one** future release. It never implicitly retains.
  An object must have at least as many valid retained references as all
  pending releases across every nested and cross-thread scope.
- At most 256 queued releases may exist at one time. The existing per-thread
  nesting, thread and lifetime token bounds still apply.
- A drain preflights all queued references for liveness, guest object header
  integrity and reference counts *before* consuming the token.
- A drained scope releases in reverse registration order and cannot be
  replayed; successful final release clears the object's private guest bytes
  and never reuses the stale address.
- Bad guest object metadata or premature external release fails closed,
  leaving the private pool open instead of reporting a successful drain.
- Operations require serialized guest execution. Full transactional undo
  in the face of concurrent mutation is not implemented or claimed.

**Not** implemented: real NSString Class, ARC fast paths, tagged pointers,
weak references, `objc_autoreleaseReturnValue`, Foundation method dispatch,
exception unwinding, real Objective-C dealloc, UIKit, or general object pools.

## Unchanged original ARM64 fixture

The existing pinned source-verifiable MIT Objective-C app executes its
original ARM64 instructions on Windows x64 Dynarmic, enters the real
empty guest pool, and reaches its first `_NSStringFromClass` call. The probe
creates its own **separate diagnostic-only private pool**, validates a
private HLE string object, queues one release and drains it. It then reports:

```
FOUNDATION_HLE_STRING_OBJECT=private-isa-not-NSString
FOUNDATION_HLE_STRING_RELEASE=verified
FOUNDATION_HLE_AUTORELEASE=private-scoped-drain-only
FIRST_RUNTIME_BLOCKER=_NSStringFromClass
```

It does not consume the unchanged app's pool token, return a private HLE
address to guest code, manufacture an `NSString`, or alter the original
binary/hash. These are diagnostic markers, not generalized API support.

## Acceptance

Portable `anyios-foundation-autorelease-tests` includes nested,
cross-thread, duplicate, forged, and replayed pool tokens; retained object
scheduled in two scopes; class-corruption/premature-release validation
failures; and correct final release. All six existing GitHub workflows,
including independent Windows ARM64 and original unchanged
Wikipedia/UIKitCatalog regressions, must pass before merging into main.

No compatibility percentage changes solely from this prototype.
