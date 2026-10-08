# Architecture decisions

## ADR-001 — Inspection before execution

Accepted 2026-10-08. Verify format and boundary behavior before loading executable guest code. Matching ARM64 CPUs do not solve Darwin ABI or framework requirements.

## ADR-002 — C++20 and zero dependencies for M0

Accepted 2026-10-08. CMake and portable C++20 spans enable host-independent parser tests. Any dependency later requires license, security and build justification.

## ADR-003 — Parser / loader / runtime / framework separation

Accepted 2026-10-08. Only parser exists; future modules cannot claim support from unimplemented stubs. Parser reports metadata, not executable readiness.

## ADR-004 — Clean-room and lawful input

Accepted 2026-10-08. Public format descriptions and owned fixtures are allowed. Proprietary binaries, firmware, cryptographic keys and DRM circumvention are excluded. AnyPS5 is GPLv2-only: review its patterns but copy none of its source.

## ADR-005 — Git and CI over project claims

Accepted 2026-10-08. Notion tracks tasks and research, but source history, versioned state, actual tests and CI decide whether something works.
