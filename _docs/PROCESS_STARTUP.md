# Owned iOS hello-world process experiment

Status: **verified original owned Windows x86-64 Dynarmic process** on 2026-10-08. Full run: https://github.com/uknes/AnyiOS/actions/runs/37766283476.

## Input

A developer-owned C program, built by clang-19/ld64.lld-19 on Ubuntu without any Apple SDK, exercises:

- A `__attribute__((constructor))` initializer that writes a distinct global sentinel before main.
- LC_MAIN-linked `main(int argc, char** argv, char** envp, char** apple)`.
- Explicit checks on argc, argv[0]/argv[1], envp and apple vectors, each with a null terminator.
- Exactly `malloc(16)`, `write(1, "hello\\n", 6)`, and `exit(23)` through registered, bounded guest ARM64 SVC thunks.

## Runtime under test

`src/process_bootstrap.cpp` prepares a 16 KiB guest-page-backed, aligned stack with bounded argument/environment/apple strings and guest pointers. It recognizes two constructor layouts:

1. Legacy `__DATA{,_CONST},__mod_init_func`: array of 64-bit pointers already rebased by the staged linker.
2. Modern `__TEXT,__init_offsets`: 32-bit offsets relative to the executable's `__TEXT` base. This format is what the current LLVM linker emitted in CI; it is **not** a missing constructor.

It validates readable descriptors, alignment/count limits and executable targets. A bounded Dynarmic host→guest callback runs initializers before entering LC_MAIN. The process is not a real Darwin kernel/dyld process: it is a deterministic owned bootstrap contract with no system frameworks, TLS, Mach IPC, threads or untrusted code execution.

## Acceptance

- SDK-free linker CI produces exact three imports and `__mod_init_func` **or** `__init_offsets`.
- All-platform CTests confirm argv/envp/apple layout, legacy and modern constructor interpretation and page refusals.
- Windows x64 Dynarmic runs complete guest constructor/main/write/exit, validates stdout and exit code from the guest.
- Only after green full job mark this a successfully executed owned process (not an arbitrary iOS app launch).

Evidence: Windows x64 `Boot owned iOS hello-world process with initializer and argv/envp/apple` step returned success, with log `Executed owned iOS LC_MAIN process with initializer and startup vectors: hello / exit 23`. This is scoped to one project-owned C fixture, not arbitrary IPA or UIKit process support. CI: https://github.com/uknes/AnyiOS/actions/runs/37766283476.