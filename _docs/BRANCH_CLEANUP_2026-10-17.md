# AnyiOS branch consolidation — 2026-10-17

All four verified PRs were merged to `main` in order:
[#39](https://github.com/uknes/AnyiOS/pull/39),
[#40](https://github.com/uknes/AnyiOS/pull/40),
[#41](https://github.com/uknes/AnyiOS/pull/41), and
[#42](https://github.com/uknes/AnyiOS/pull/42).

The post-merge `main` tree was verified byte-for-byte identical to the
verified #42 tip tree (tree SHA
`45c5c753bb3b387a5f1195adc62fcb025c05eab2`).
No outstanding tested changes from these PRs need a parallel feature branch.

## Branches preserved for separate review

These branches contain history not reachable from `main`. They are **not**
automatically deleted or merged. Preserving a branch does not mean its
old code is safe to merge into current main.

- `docs/compatibility-tiers-gap-plan` — `39c22c7b2a14cde7c7bda363caaaa5e8165c1675`
- `feature/dynarmic-guest-tls` — `dc8f35e7117224a478797f72f396f7bb111b5a20`
- `feature/first-ios-app-gap-probe` — `bcd3dd32bd29a81999b273becff28176939a2b2b`
- `feature/full-compatibility-treemap-and-sasquatch-intake` — `38df379ca384afb9bc8f750d8ea3ca68dae88cd1`
- `feature/libsystem-owned-memory-primitives` — `53b6521de5f2573bea6e5f7f27a81365476c91be`
- `feature/objc-typed-instance-dispatch` — `2463572c70f334e713314bf6672bf6eadbeae678`
- `feature/windows-interactive-arm64-framebuffer-2048` — `ee6afd93ddddd8c1ffab46847425ec2cd1260d37`

## Redundant refs eligible for deletion

The exact branch refs and commit SHAs below are a recovery manifest, not
separate code changes. The cleanup workflow rechecks each ref against the
pinned expected commit SHA and confirms that the current head is reachable
from `main` immediately before deletion. If a ref has moved, become
protected, is already deleted, has an open PR, or is not an ancestor, it is
skipped. Each ref can be recreated from its SHA using
`git push origin <commit-sha>:refs/heads/<branch>` by an authorized maintainer.

- `feature/bounded-external-ios-entry` — `91406fcde6765d115ba043fe8cf134b839cfde59`
- `feature/bounded-large-symbol-tables` — `3b32cbbefc5eee6852045338bd77f0474b5b4ad9`
- `feature/bundle-dependency-plan` — `aa481c529009d98f17df9590d82e2316616ddde3`
- `feature/external-runpath-intake` — `4a63862d4f08410df4f72d7d5c3719e381da2362`
- `feature/foundation-class-name-utf16-20261016` — `20634836a066af857db66f3733cd3c7d5604e1f6`
- `feature/foundation-hle-string-objects-20261016` — `698b65735b44cbd6fd62764898cd62d6b3c8148e`
- `feature/guest-dlsym-scope` — `dcfe1a16a62fe5a2f771b579511d64059b3efebf`
- `feature/guest-getenv-from-envp` — `2abadf3f5e2dcefe4118957c8c83fddd588e5923`
- `feature/guest-os-log-create` — `5259f11119ecd0653bf708515976ffef00b0d726`
- `feature/guest-os-log-type-enabled` — `8d93e8014220d3a265af1ad9374b751e2c7167e9`
- `feature/ios-uicatalog-original-intake` — `e2818f3ab29d12746a70cadd2324d92cb223d31f`
- `feature/legacy-bind-modular-cursor` — `aa8d231322e02eb5817b5ea35ad4848ac89e41b6`
- `feature/legacy-dyld-bind-opcodes` — `056239ee71de3a2508e531d0d8006d5c2641913c`
- `feature/legacy-dyld-metadata` — `2e30c3738ac76e70827ee127a0af077d8d47f41c`
- `feature/legacy-dyld-rebase-opcodes` — `67f1d69d47475b2eef86cfe8bc7015f7a2f6a48c`
- `feature/legacy-transactional-relocation` — `7d9c9d893283adee135ce729d994343d1ce915dc`
- `feature/legacy-weak-lazy-bind` — `27ee0d24605c112dd501540a16e1a047f7554389`
- `feature/libsystem-memory-string` — `84aac9aed799e6f01059604c11d6842eb4c113b3`
- `feature/native-memory-contract-ci` — `c1414efd8e98e5e03d8727f54a313aed95840a2f`
- `feature/objc-empty-pool-entry-probe` — `b482c06a898565d9dcd10c22d2458ba6018bf8fb`
- `feature/objc-general-class-identity-20261016` — `414d18f5c6be4811b0d690964030f59ad4d550cb`
- `feature/objc-guest-autorelease-pool-runtime-20261016` — `c84dc4e7f453636ff5f9f12f77c1fc303ce935fb`
- `feature/objc-guest-method-abi` — `18b85f28b415ea4359a789c13b88f5a971fff8b1`
- `feature/objc-guest-object-arena` — `8c65fd87ee78f2ada4f2556b7b2546ed5dae337f`
- `feature/objc-lifecycle-callback-probe` — `9c760ceec0ea1927bab65f4614881bde4dbba669`
- `feature/objc-local-method-resolution` — `8267a63b2fb6479181738080e4f0710c66102673`
- `feature/objc-local-selector-identity` — `691ffa69f6f5b46d75ad729a85f6d189418b7864`
- `feature/objc-owned-bool-dispatch` — `f48cf8bb320ef467755d3b22f8517a9f762fbab7`
- `feature/objc-static-class-registry` — `cf3a48b337a999095c94c50aee3375738d2cb409`
- `feature/owned-multi-image-initializers` — `99836c461430c0be93c87706a75153a67dcee17d`
- `feature/uicatalog-windows-original-entry` — `4adc32b41aaf5b6cdaedd3f02d789419cfe5cbc5`
- `feature/wikipedia-original-guest-execution` — `85ebe5044bde364db19cfb028a1a93b2d86f9172`
- `feature/wikipedia-original-ios-coverage` — `ef2627cced9892685d91493377734c13c2ff922c`
- `feature/wikipedia-winarm64-x64-guest-run` — `2e6cc37e99d7f29f584ddbb62b81e909f0b243bc`
- `feature/15min-compat-watch` — `6440654a941ad7878e73a825ba6e7a6e7068687d`

## Safety policy

Never delete `main`, a branch with uniquely unmerged commits, a protected
branch, an open PR's head/base branch, or a branch whose head SHA changed.
Cleanup should make no changes to source files. The seven preserved branches
will be revisited separately and can be archived after their useful work is
carried forward intentionally.

## Subsequent verified milestone — PR #43

[PR #43](https://github.com/uknes/AnyiOS/pull/43) merged scoped private
Foundation HLE autorelease into `main` at
`c9857ae0d06d44cf7a158cb394aba03da8d2b208`.
The merge-tree SHA `8a09cc51f9d789a79a94b2530743bea50eb83d1b`
was verified identical to the exact-head six-workflow-passing PR branch.
The now-redundant `feature/foundation-runtime` ref was pinned at
`10a88372bff0e660f946b65f74961399388abd62` for safe automated
cleanup. Its commits are retained in `main`.
