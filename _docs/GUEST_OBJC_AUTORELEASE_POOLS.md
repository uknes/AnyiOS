# Bounded guest Objective-C empty autorelease pools

This milestone extracts the source-vetted app-entry probe's limited
`_objc_autoreleasePoolPush` / `_objc_autoreleasePoolPop` behavior into a
standalone runtime component shared by the specialized and general probes.

## Precise implementation contract

`GuestAutoreleasePools` maps one 16 KiB, guest-readable but non-writable and
non-executable page for **opaque** 16-byte-aligned tokens. It supports up to
1,024 pushes in one lifetime, up to 16 active nested pools per sequentially
emulated thread and up to 64 concurrently tracked guest thread identities.

- Each successful push produces a new, never-reused guest token.
- Pop requires the top token on the same guest thread. Forged, stale, repeated,
  cross-thread and out-of-order tokens are rejected without changing state.
- Existing empty stacks can be reused, but exhausted token capacity cannot.
- A guest thread with open pools cannot pass the `finish_thread` check.
- The arena must be aligned, non-overlapping and inside the bounded guest
  address space. A collision aborts the probe.
- These methods are **not synchronized**: the current owned execution
  engines serialize guest operations. Concurrent guest threads remain unsupported.

This is **only an empty pool implementation**. Autoreleased objects, retain
counts, deallocation order, `objc_autorelease`, `objc_release`, exception
unwinding, real Objective-C runtime pools and general `Foundation` are
not implemented. No host pointers or actual Apple frameworks are provided.

## Original unchanged app acceptance

The general Windows x64 Dynarmic entry probe must execute original ARM64
instructions from the exact hash-verified pinned MIT Bitrise Objective-C app.
It must report `SUPPORTED_NARROW_IMPORT=_objc_autoreleasePoolPush` before
stopping at the genuinely unsupported `_objc_msgSend`. The original guest
binary is not edited or relinked for this result.

The specialized probe continues to execute its already verified, narrow
guest AppDelegate BOOL callback and stops at `_UIApplicationMain`, without
claiming that UIKit, a window or an iOS app launch exists.

Regression tests exercise depth/thread/token limits, malformed values,
read-only guest memory, map collision, empty drain and ABA/replay prevention.
CI includes the contract as a portable CTest and runs it explicitly on the
pinned Windows x64 entry-probe job. Any change to the original-app runtime
boundary must retain the original SHA-256 identity and failure point.

No API coverage or end-user application compatibility gate is promoted
solely by this bounded implementation.
