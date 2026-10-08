# Security policy

## Current execution trust boundary

**AnyiOS is experimental. It cannot safely run arbitrary iOS apps or IPA packages.**

The Windows ARM64 native linked-code executor runs **inside the AnyiOS test process**. It is enabled only for compiler-generated, project-owned fixtures in CI. It is **not sandboxed**. A Mach-O parser accepting a binary does not make executing it safe.

**Untrusted binaries must not enter the native executor.** Until process isolation, exception handling, reliable Darwin SVC interception, ABI call gates, and memory protections are in place, investigate untrusted guest instructions only through the Dynarmic translated backend with explicit guest-memory callbacks and bounded execution. Even that path is research infrastructure, not a security-certified sandbox or a general app launcher. Do not assume Dynarmic alone makes malicious app files safe.

### Specific constraints

- Native executable regions are taken from one reserved, 64 KiB-aligned Windows arena; only validated guest segments are committed. Mapped guest pages are 16 KiB logical units with final RX, RO, or RW access. Non-mapped holes remain uncommitted guard regions.
- The native fixture runner scans each executable mapping before changing protections to RX. SVC-looking literal-pool data is rejected too. A static scan cannot protect against dynamic code generation and is not a substitute for process isolation.
- No Apple SDK/runtime files, DRM keys, decrypted application code, or proprietary frameworks are shipped.
- Unknown dylib symbols, pointer formats, syscall numbers, variadic ABI structures, pointer/aggregate thunks, and guest↔host callbacks outside the tested contract must fail explicitly.
- Keep MIT licensing and do not copy GPL/LGPL/APSL implementation code.

### Vulnerability reporting

Please avoid publishing exploit details against in-development sandbox components before maintainers can triage them. Open a GitHub security advisory in the repository if available, or contact the maintainer privately via the repository's listed contact channel. Never submit real secrets, credentials, or proprietary IPA contents in test reports.

Security claims follow only successful CI tests and reviewed, committed source.
### Guest thread registers and TLS

No guest Darwin TLS is implemented. Dynarmic MRS/MSR guest instructions are now refused until per-thread TPIDRRO_EL0 state is available. The trusted native executable mapping scan refuses MRS/MSR words as well as SVC, including lookalike literal data. This guard is not safe native system-register interception, and untrusted native guest code remains prohibited. See [_docs/TLS_DESIGN.md](_docs/TLS_DESIGN.md).
