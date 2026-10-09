# Generic ARM64 iOS executable entry — first runtime blocker only

New target anyios-general-entry-probe executes a source-verified, unprotected original ARM64 MH_EXECUTE using Dynarmic on Windows x86-64. It does not use Bitrise-specific ObjC dispatch tricks. Limits: 128 MiB binary, 384 MiB bounded guest memory, 8192 import traps, 10000 instructions. Every unresolved imported function uses a guest ARM64 trap that records the first original imported symbol and stops. No fake Foundation/UIKit/Swift implementations.

The existing Bitrise-specific probe remains separate and may execute a verified local AppDelegate BOOL IMP. General entry must not be described as equivalent to original iOS app launch: no dependent dylib initializers, original Foundation/Swift library semantics, UIKit, Windows window, native Windows ARM64 guest process, nor touch. Unknown events and loader layouts fail closed.

Security: trusted, legally redistributable source-built binaries only. This is an in-process emulator/JIT and not a sandbox for untrusted app packages. The executable never modifies its input.

Acceptance: exact unchanged pinned Bitrise original MIT ARM64 binary must stop at _objc_autoreleasePoolPush using the generic probe and record a positive bounded step count. Apply this code to the original Wikipedia ARM64 app only after its exact pinned Xcode output and dependency report are verified; any unsupported larger Mach-O or missing dyld is a blocker, not false app success.
