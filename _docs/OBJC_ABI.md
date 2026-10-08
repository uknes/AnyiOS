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
