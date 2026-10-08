# AnyiOS

Experimental clean-room compatibility research for unprotected ARM64 iOS application binaries on non-Apple platforms.

**Current milestone: binary inspection only.** AnyiOS does not run iOS applications, decrypt commercial apps, implement UIKit, or provide an iOS runtime.

Build: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build && ctest --test-dir build --output-on-failure`.

See `_docs/` for architecture, research notes, security policy, decisions, and milestone state.

Source: https://github.com/uknes/AnyiOS
