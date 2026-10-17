# Narrow original ARM64 Objective-C class identity — research and acceptance

## Research and compatibility boundary

Apple's public documentation defines `+[NSObject class]` as returning the
class object, and `objc_msgSend` as a runtime message sent with a receiver
and selector. Apple's runtime resolves implementation by class/method data;
GNUstep libobjc2 is useful for ABI architecture research but is **not a
drop-in Apple iPhoneOS runtime**. Public references:

- https://developer.apple.com/documentation/objectivec/nsobject-swift.class/class
- https://developer.apple.com/documentation/objectivec/objective-c-runtime
- https://github.com/apple-oss-distributions/objc4
- https://github.com/gnustep/libobjc2

The general Windows x64 original-entry probe lazily initializes a validated
Clang classlist/selector registry only if an original ARM64 guest actually
calls an Objective-C runtime import. `_objc_msgSend` supports only the local
`+class` identity operation and only for a correctly registered guest class
receiver and original compiler-emitted selector. It returns the guest class
object address, never a host class or fabricated UIKit object.

`_objc_getClass` can find *registered local* classes using a validated guest
C-string (maximum 127 printable bytes). Unregistered or imported framework
classes are unsupported. This does not implement general class lookup,
class initialization, categories, metaclasses, custom `+class` overrides,
method forwarding, `objc_msgSendSuper`, typed return and argument marshalling
or actual Objective-C runtime dispatch.

`_NSStringFromClass` is **not implemented**. The general probe may log a
validated local class name for diagnosis, but returns a precise
`FIRST_RUNTIME_BLOCKER=_NSStringFromClass` rather than constructing a fake
NSString. This is intentional: NSString is a Foundation object with lifecycle,
type identity, and layout requirements. The older specialized pinned-only
AppDelegate diagnostic retains its `NSStringFromClass` *token bridge* solely
inside its explicitly isolated probe and does not claim an actual NSString.

## Original-app and portable verification

Portable `anyios-objc-dispatch-tests` now check registered class identity,
reject unrelated selectors, forged local class addresses, invalid selectors,
and preserve existing safe guest BOOL callback behavior. The pinned original
MIT Objective-C app is rebuilt in each GitHub Actions run from the pinned
upstream source and verified against the unchanged recorded SHA-256.

The general probe must execute the actual unchanged guest ARM64 instructions:
`_objc_autoreleasePoolPush` -> `_objc_msgSend` (local `+class` identity) ->
`FIRST_RUNTIME_BLOCKER=_NSStringFromClass`. No original binary patching,
no fake UIKit renderer, no released-object acceptance. The specialized probe
must independently keep running its narrow original AppDelegate BOOL IMP and
stop at `_UIApplicationMain`.

Wikipedia and UIKitCatalog unchanged-app workflows must continue to pass;
their first blocker can differ because they have distinct dependency graphs.
All CI is required before the PR becomes ready. This does not promote the
verified API-export/gate counters or imply native Windows ARM64 iOS
application startup or working graphics.
