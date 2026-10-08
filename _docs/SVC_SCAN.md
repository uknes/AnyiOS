# Native ARM64 SVC preflight assumptions

Status: implemented for original trusted linked iPhoneOS fixture; CI verification pending.

- Before each new native executable mapping, scan **all bytes of all guest pages** in the mapping in 4-byte instruction-sized words. Reject SVC opcodes with any immediate.
- No discriminator can reliably distinguish code from embedded literal pools without executing or validating control flow. A literal with an SVC-shaped word **must fail closed**, even when it is data.
- Scan before host memory is committed and switched from RW to RX. The owned-fixture runner never writes or remaps those pages once they become executable. **Any future executable mapping must be scanned anew.** Self-modifying executable code and dynamic re-protection remain unsupported.
- A scan is NOT a native sandbox or secure SVC interception mechanism. Untrusted iOS machine code is forbidden on the native in-process backend.
- Windows x64 Dynarmic execution uses bounded guest-memory callbacks and an SVC event, rather than allowing guest SVC to execute on host.

Regression test: `native-svc-preflight`; native ARM64 linked fixture is scanned at each executable segment publication.