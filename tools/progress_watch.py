#!/usr/bin/env python3
"""Read-only snapshot of scoped iOS compatibility and current GitHub CI."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sys
from urllib.parse import quote
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parent))
import progress


def run_states(runs, branch):
    return [
        (r.get("name", "Unknown"), r.get("status", "unknown"),
         r.get("conclusion") or "pending", r.get("html_url", ""))
        for r in runs if r.get("head_branch") == branch
    ][:3]


def render(state, prs, runs, timestamp):
    a, g = state["libraries"], state["runtime"]
    lines = [
        "# AnyiOS 15-minute CI and status watch",
        "",
        f"Checked: {timestamp} UTC (scheduled GitHub Actions jobs may be delayed).",
        "Read-only CI snapshot; **not** proof of any newly implemented API or app.",
        "",
        "| Inventory | Verified | Partial | Pending | Tracked |",
        "|---|---:|---:|---:|---:|",
        f"| iOS candidate API exports | {a['done']} | {a['partial']} | {a['pending']} | {a['total']} |",
        f"| Compatibility gates | {g['done']} | {g['partial']} | {g['pending']} | {g['total']} |",
        "",
        "Inventories are explicitly selected work items, not total iOS compatibility.",
        "",
        "## Open PRs / recent matching Actions runs",
    ]
    if not prs:
        lines.append("No open PRs.")
    for pr in prs[:20]:
        branch = pr["head"]["ref"]
        lines.append(f"- [PR #{int(pr['number'])}]({pr['html_url']}) ({'draft' if pr.get('draft') else 'open'})")
        items = run_states(runs, branch)
        if not items:
            lines.append("  - No recent matching CI run in the fetched window.")
        for name, status, conclusion, url in items:
            lines.append(f"  - {name}: {status} / {conclusion} ([GitHub run]({url}))")
    lines += [
        "",
        "Verification requires opening the exact run/head SHA, checking guest",
        "execution output and testing the original binary. This watch neither",
        "writes code nor modifies GitHub PRs/Notion.",
    ]
    return "\n".join(lines) + "\n"


def github_json(base, path, token):
    req = Request(base.rstrip("/") + path, headers={
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "AnyiOS-compat-status-observer",
    })
    with urlopen(req, timeout=20) as response:
        return json.load(response)


def main():
    repo = os.environ.get("GITHUB_REPOSITORY")
    token = os.environ.get("GH_TOKEN")
    if repo != "uknes/AnyiOS" or not token:
        raise SystemExit("This read-only monitor requires the AnyiOS GitHub token")
    api = os.environ.get("GITHUB_API_URL", "https://api.github.com")
    prefix = "/repos/" + "/".join(quote(s, safe="") for s in repo.split("/"))
    prs = github_json(api, prefix + "/pulls?state=open&per_page=30", token)
    runs = github_json(api, prefix + "/actions/runs?per_page=70", token)
    text = render(progress.collect(progress.REPO), prs, runs["workflow_runs"],
                  datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S"))
    print(text)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as output:
            output.write(text)


if __name__ == "__main__":
    main()
