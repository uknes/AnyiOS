# Objective-C ABI — clean-room notes

## Objective and hard boundaries

AnyiOS is an ARM64 iOS Mach-O binary compatibility runtime for Windows, not a
source port into UWP. Do not read or copy GPL, LGPL or APSL implementations.
The ObjC ABI is derived from the project's own Clang-compiled binaries and
permissive public documentation, not another emulator's source.

## Experimental scopes, never full API claims

- Autorelease pool: entry-probe-only empty-pool push/pop, strict LIFO, depth
  bounded at 16. Autoreleased objects, cleanup and destructors unsupported.
- ObjC dispatch: one validated class identity selector (+class), whose
  receiver is listed in the local compiler-emitted classlist and whose
  selector text is in objc_methname. Arbitrary objc_msgSend unsupported.
- Class metadata: bounded interpretation of local ObjC2 non-fragile class
  records and a 24-byte absolute method-list encoding. Metadata must be
  readable from the guest mapped const sections, IMP executable, selector
  and class-name strings bounded to their declared sections. There is no
  runtime class registration, superclass inheritance, category/protocol
  resolution or selector interning.
- NSStringFromClass: a probe-only opaque name-handoff token returned when
  local class metadata is validated. This is NOT an NSString object and
  cannot receive ObjC messages. It is not general Foundation support.
- UIApplicationMain: NOT implemented. A bounded external app diagnostic
  can call the original ARM64 AppDelegate didFinishLaunchingWithOptions IMP
  with inspected fixed scalar arguments, outside any genuine UIKit lifecycle.
  It does NOT create a UIApplication, load a storyboard, paint a window, or
  process touch events. An executed callback is not proof of app launch.

## Real launch requirements

1. Proper guest class and selector registry with Objective-C method dispatch,
   inheritance, lifecycle initializers and parameter/return ABI.
2. Guest object allocation and memory ownership, NSString and minimal
   Foundation, autorelease semantics and exception/weak-reference contracts.
3. UIKit application delegate instantiation and lifecycle, UIWindow,
   UIViewController and graphical rendering with native Windows input.
4. A reproducible third-party open-source app must display its OWN view
   correctly and respond to events before claiming a T2/T3 app launch.

## Design-only references

- Clang Objective-C runtime configuration (Apache-2.0 with LLVM exception):
  https://clang.llvm.org/doxygen/ObjCRuntime_8h_source.html
- Objective-C messaging ABI documentation:
  https://developer.apple.com/documentation/ObjectiveC/objc_msgSend
- touchHLE architecture: https://github.com/touchHLE/touchHLE
  (MPL-2.0, no source copying; targets early 32-bit iOS, not modern ARM64).
- WinObjC: https://github.com/microsoft/WinObjC
  (MIT, archived, source-porting bridge rather than iOS binary emulation).

## Hot-path ADR still pending

Do not commit to SVC per-call for objc_msgSend or retain/release before
measuring host-to-guest crossing costs against a guest-side fast path on
actual trace workloads. Guard guest pointers and fail closed on unknown ABI.

## Bounded guest instance milestone (stacked on PR #6)

`GuestObjcObjectArena` allocates 16-byte-aligned, zero-filled instances using
only the validated local Clang class_ro_t `instanceSize` and the original guest
Class address as `isa`. The test arena is one iOS page, fail-closed on
unrecognized classes and size bounds. A refcounted last release clears the
guest bytes and permanently retires that address within the arena. It does
not run ObjC dealloc, destructors, weak cleanup, or autorelease callbacks.
This supports the pinned Bitrise AppDelegate *diagnostic* callback with an
allocated guest object rather than a hardcoded object pointer. It is NOT
`objc_alloc` compatibility, a UIApplication, an app-owned UIWindow, or a GUI.

## Local compiler class registration checkpoint (PR #11)

`GuestObjcClassRegistry` indexes only owned, validated local Clang ObjC2
`__objc_classlist` class records by their original guest addresses and
`class_ro_t` names, rejects duplicate/malformed registrations, checks bounded
guest C-string lookup, and resolves superclasses only when locally registered
(or explicitly null). External UIKit/Foundation superclass references must
remain unresolved rather than converted into a fake local root. This is a
narrow prerequisite for `_objc_getClass`, not a complete libobjc dispatcher,
metaclass registry, initializer runner, or true Foundation object.

Both compiler-emitted `__TEXT,__objc_classname` and ordinary `__cstring`
class name section layouts are accepted by the bounded class reader. Other
unrecognized metadata forms remain unsupported until tested.


## Original Bitrise local-class registry correction

The pinned MIT Bitrise binary can expose compiler class records whose
`class_ro_t` class-name or instance-size metadata is not supported by this
narrowly validated parser. A previous strict constructor rejected the entire
app on the first such record and prevented the already-proven AppDelegate
ARM64 callback. The registry now **rejects individual unrecognized records**,
counts them in `unresolved_count()`, and publishes only classes with a readable
superclass, validated local name, and bounded instance size. Unknown records
cannot be used for dispatch, allocation, superclass inference or class lookup.
Duplicate **valid** names remain an error. This is not full class registration.

The Windows x64 pinned original-app test must report both resolved and
unresolved counts, execute the real AppDelegate callback, and explicitly stop
at the unimplemented `UIApplicationMain`.

## Narrow local Objective-C instance-method lookup (PR #13 candidate)

The class registry can resolve an instance method from a validated local
Clang ObjC2 absolute method list, searching local superclass records only.
It returns the original ARM64 guest IMP and selector address: execution
still requires the existing guarded guest callback ABI path. Subclass
implementations take precedence. Missing selectors, unknown external
superclasses, unregistered classes and cyclic local inheritance fail closed.
Only 24-byte absolute method-list entries are recognized by the underlying
ObjcIdentityProbe; relative method lists and method-list variations are not
reinterpreted or silently fabricated. This does not implement arbitrary
objc_msgSend, forwarding, dynamic method resolution, metaclasses, categories,
class initialization, or UIKit. The unchanged Bitrise AppDelegate callback
continues to be a diagnostic guest execution only.

## Bounded original compiler selector identity (next step after PR #13)

`GuestObjcSelectorRegistry` canonicalizes a maximum of 256 validated
compiler-emitted `__objc_methname` strings to original guest pointer
addresses. Repeated selector text receives the first registered original
address, with the canonical guest string revalidated before reuse. Unknown,
unterminated, stale, unmapped and non-method-section guest selector values
fail closed. This narrow interner is wired to the original Bitrise ARM64
`+[AppDelegate class]` diagnostic `_objc_msgSend` entry path, and cannot
materialize Windows-side substitute selectors or arbitrary host pointers.

The selector table does **not** implement `sel_registerName`, `sel_getName`,
`__objc_selrefs` patching, dynamic method lookup, general class/instance
message dispatch, cross-module registration or UIKit. Its verified scope is
one prerequisite for guest-owned selector identity only.

## Compiler method ABI gate — narrow BOOL delegate callback

Apple's runtime exposes the method type encoding separately from the original
ARM64 IMP. `ObjcIdentityProbe` now reads type-encoding strings only from mapped,
bounded `__objc_methtype` or compiler `__cstring` sections. Unknown encoding
pointers are reported as absent, not accepted. `supported_bool_launch_abi`
accepts only the exact compiler scalar signatures `c32@0:8@16@24` and
`B32@0:8@16@24` for the pinned four-register AppDelegate diagnostic,
rejecting unsupported aggregates, variadics, offsets and qualifiers.

The original Bitrise ARM64 method is permitted through the existing guarded
callback only if its actual metadata matches. This is **not** a complete
`objc_msgSend` implementation or a general method type-encoding parser;
`UIApplicationMain`, class initialization, dispatch, real Foundation and
UIKit remain unsupported. Positive evidence requires pinned original-app
Dynarmic CI, not just portable tests.

Primary research: https://developer.apple.com/documentation/objectivec/method_gettypeencoding%28_%3A%29
and https://developer.apple.com/videos/play/wwdc2020/10163/ .

## Object-owned guest BOOL method dispatch probe

The bounded dispatcher checks a live GuestObjcObjectArena instance, validates
its guest-memory `isa` against the original allocation's class, resolves
only original compiler-mapped selector strings and local class/ancestor method
metadata, and verifies the exact BOOL delegate ABI before entering the
original ARM64 guest IMP through `invoke_guest_callback`. The diagnostic
passes only nil UIApplication and launch options, as independently inspected
for the pinned Bitrise sample, and enforces BOOL values 0 or 1. Released,
forged, externally registered or isa-corrupted objects are refused. The
portable backend mock proves dispatch invariants only; the pinned original
Bitrise Dynarmic regression alone proves real ARM64 instructions executed.

This does not provide general `objc_msgSend`, class methods, +initialize,
Foundation UIApplication, NSString, UIWindow, touch events or original guest
pixels. It is not an application launch and does not justify promoting any
full compatibility/API gate.
