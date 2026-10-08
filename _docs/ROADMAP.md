# Milestones

## M0 — Passive Mach-O inspector

- [x] ARM64 thin header and load-command parsing
- [x] Segment, dependencies, entry, runpath, deployment versions and encrypted-code indicators
- [x] Synthetic invalid-input and mutation tests
- [x] GCC and Clang ASan/UBSan local builds
- [ ] GitHub Actions verified across Linux and another host

## M1 — Expanded image analysis

- [ ] Select ARM64 universal/fat binary slices safely
- [ ] Parse dependency, section, dyld chained fixup and export metadata
- [ ] Parse project-owned app bundle and Info.plist
- [ ] Differential validation against independently compiled own Mach-O examples
- [ ] Add coverage-guided fuzzing and corpus governance

## M2 — Controlled owned-binary execution

- [ ] Host ARM64/Linux design for Darwin process ABI, syscalls, linking and exceptions
- [ ] Minimal safe mappings and relocations for a project-owned CLI executable
- [ ] Demonstrate owned hello-world guest program with documented constraints

## M3 — Runtime contracts

- [ ] libSystem and Darwin API contract tests
- [ ] Objective-C/Swift runtime strategy and executable samples

## M4 — Minimal app lifecycle

- [ ] Original iOS sample bootstraps its window and input
- [ ] Verifiable supported-host and API compatibility table

Not promised: encrypted retail content, online Apple services, arbitrary iOS releases, modern commercial games or x86 execution.
