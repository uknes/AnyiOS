# Automated progress observation

[15-minute GitHub Actions workflow](../.github/workflows/progress-watch.yml)

GitHub Actions uses the cron expression `*/15 * * * *` on the main
branch to check the scoped status graph and fetch the most recent open PR
and Actions run statuses. It emits a **read-only job summary**.
The GitHub schedule may run late or skip an interval.

The watcher does not execute iOS apps, automatically merge, modify source,
push status changes, or edit the Notion workspace. It cannot attest to
new API support merely from green workflow checks. CI-backed manual changes
to tools/api_manifest.json and tools/compat_capabilities.json remain the
only basis for the README graph.

This is a GitHub-side progress check, *not* 15-minute ChatGPT execution.
The ChatGPT development task's minimum interval is one hour.
