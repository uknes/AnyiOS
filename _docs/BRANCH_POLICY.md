# AnyiOS branch policy

Keep one integration branch (`main`) and **at most one active development
feature branch** at a time. Do not create an additional branch just to
continue one topic, run a CI test, or implement a dependent piece of the
same milestone. Continue committing to the current active branch.

1. Create one short-lived feature branch from latest `main` for a cohesive
   milestone. Keep all development and regression fixes in that branch.
2. Run portable unit tests, native Windows ARM64 contracts, Windows x64
   Dynarmic guest tests, unchanged Wikipedia/UIKitCatalog tests and pinned
   original Objective-C checks where applicable. Claim only verified gates.
3. Open one PR to `main`. Do not create stacks of four or more PRs with
   branch-on-branch dependencies.
4. After CI and review, merge to `main`, ensure identical expected code,
   and delete the merged branch. Avoid deleting unmerged work.
5. Research or risky GUI experiments belong in documented existing archive
   branches until their contents have been reviewed; don't merge ancient
   experimental branches solely to remove clutter.
6. Do not force-push `main` or squash away unique unmerged history.

Enable **Settings → General → Pull Requests → Automatically delete head
branches** on GitHub where allowed, so merged branches disappear after PR
merges. This setting is not modified by the current connector.

The initial 35-branch cleanup was completed using an audited SHA allowlist,
with recovery SHAs in `_docs/BRANCH_CLEANUP_2026-10-17.md`. For future work,
`.github/workflows/merged-branch-hygiene.yml` runs after merged pull requests
and deletes only same-repository feature branches that have no open PR, retain
their expected merge-time SHA, and contain no commits absent from `main`.
The workflow never force-pushes or deletes the default branch.

No branch proliferation: prefer updating an existing current feature branch.
