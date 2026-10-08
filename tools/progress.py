#!/usr/bin/env python3
"""Source-owned AnyiOS compatibility reporting.

Provides the same *kind* of output as AnyPS5's status generator:
two percentage badges, a grouped function-tile treemap, structured JSON,
a browsable status table, and a before/after change report.

Independent implementation; no AnyPS5 GPL-2.0 source is incorporated.
The denominator is the explicitly tracked API/capability inventory,
NOT the number of functions or features in all iOS versions.
"""
import argparse
from collections import Counter
from html import escape
import json
import math
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parents[1]
WIDTH = 1000
PANEL_WIDTH = 495
GAP = 10
TOP = 34
PANEL_HEIGHT = 300
DARK = "#0d1117"
SUCCESS = "#2ea043"
PARTIAL = "#d29922"
PENDING = "#6e7681"
WHITE = "#ffffff"
SOURCE = "https://github.com/uknes/AnyiOS/blob/main"


def require(condition, problem):
    if not condition:
        raise ValueError(problem)


def data_file(root, name):
    return json.loads((root / "tools" / name).read_text(encoding="utf-8"))


def summarize(groups, source, unit):
    done = sum(len(g["done_names"]) for g in groups)
    partial = sum(len(g["partial_names"]) for g in groups)
    total = sum(g["total"] for g in groups)
    return {
        "source": source, "unit": unit, "done": done, "partial": partial,
        "pending": total - done - partial, "total": total,
        "percent": round(100 * done / total, 2) if total else 0,
        "groups": groups,
    }


def collect(root):
    manifest = data_file(root, "api_manifest.json")
    api = data_file(root, "api_inventory.json")
    runtime = data_file(root, "compat_capabilities.json")
    require(manifest.get("schema") == api.get("schema") == runtime.get("schema") == 1,
            "incompatible manifest schema")
    declared = manifest["symbols"]
    seen = set()
    api_groups = []
    for entry in api["groups"]:
        names = entry["symbols"]
        require(len(names) == len(set(names)) and not seen.intersection(names),
                "duplicate API symbols in groups")
        seen.update(names)
        done, partial, pending = [], [], []
        for name in names:
            meta = declared.get(name, {})
            status = meta.get("status", "unknown")
            require(status in ("implemented", "partial", "not implemented", "unknown"),
                    "unsupported symbol status " + name)
            if status in ("implemented", "partial"):
                require(isinstance(meta.get("scope"), str) and meta["scope"].strip(),
                        "implemented/partial API lacks its exact evidence scope: " + name)
            ("implemented" == status and done or
             "partial" == status and partial or pending).append(name)
        api_groups.append({
            "name": entry["name"], "label": entry["name"],
            "total": len(names), "done_names": sorted(done),
            "partial_names": sorted(partial), "todo_names": sorted(pending),
        })
    require(set(declared) <= seen, "every manifest symbol must appear in the API inventory")
    seen_caps = set()
    cap_groups = []
    for group in runtime["groups"]:
        done, partial, pending = [], [], []
        for task in group["items"]:
            name = task["name"]
            require(name not in seen_caps, "duplicate capability name: " + name)
            seen_caps.add(name)
            status = task["status"]
            require(status in ("verified", "partial", "pending"),
                    "invalid capability status: " + name)
            if status != "pending":
                require(task.get("evidence", "").strip(),
                        "completed/partial capability missing evidence: " + name)
            (done if status == "verified" else
             partial if status == "partial" else pending).append(name)
        cap_groups.append({
            "name": group["name"], "label": group["name"],
            "total": len(group["items"]), "done_names": sorted(done),
            "partial_names": sorted(partial), "todo_names": sorted(pending),
        })
    require(len(seen) >= 9 and len(seen_caps) >= 100,
            "incomplete inventory; do not report inflated percentages")
    return {
        "schema": 2,
        "notes": [
            "Percentages apply ONLY to explicitly tracked selected API exports and declared compatibility gates.",
            "Selected API examples do NOT enumerate Apple's full frameworks or every iOS release.",
            "Partial work does not count toward the implemented/verified percentage.",
            "An API's missing evidence means unverified, not necessarily proven impossible.",
            "No iOS app or game is rated playable merely from a symbol or feature count.",
        ],
        "libraries": summarize(api_groups, "tools/api_inventory.json", "API exports"),
        "runtime": summarize(cap_groups, "tools/compat_capabilities.json", "capability gates"),
    }


def tile(x, y, w, h, color, border=0.55):
    return (f'<rect x="{x:.2f}" y="{y:.2f}" width="{max(0, w):.2f}" '
            f'height="{max(0, h):.2f}" fill="{color}" stroke="{DARK}" '
            f'stroke-width="{border:.2f}"/>')


def note(x, y, label, size=12):
    return (f'<text x="{x:.2f}" y="{y:.2f}" font-size="{size}" '
            f'font-family="Arial,DejaVu Sans,sans-serif" fill="{WHITE}" '
            f'paint-order="stroke" stroke="{DARK}" stroke-width="2.5">'
            f'{escape(label)}</text>')


def partition(groups, x, y, width, height):
    """Deterministic balanced weighted rectangle partition (clean-room)."""
    if not groups:
        return []
    if len(groups) == 1:
        return [(groups[0], x, y, width, height)]
    weights = [g["total"] for g in groups]
    grand = sum(weights)
    target = grand / 2
    accumulated = 0
    split = 1
    closest = math.inf
    for i in range(1, len(groups)):
        accumulated += weights[i - 1]
        distance = abs(target - accumulated)
        if distance < closest:
            closest, split = distance, i
    a = groups[:split]
    b = groups[split:]
    ratio = sum(g["total"] for g in a) / grand
    if width >= height:
        width_a = width * ratio
        return (partition(a, x, y, width_a, height) +
                partition(b, x + width_a, y, width - width_a, height))
    height_a = height * ratio
    return (partition(a, x, y, width, height_a) +
            partition(b, x, y + height_a, width, height - height_a))


def subcells(total, x, y, width, height):
    if total == 0:
        return
    rows = max(1, min(total, round(math.sqrt(total * height / max(width, 0.1)))))
    first = 0
    for row in range(rows):
        count = total // rows + (row < total % rows)
        if not count:
            continue
        dx = width / count
        dy = height / rows
        for column in range(count):
            yield x + column * dx, y + row * dy, dx, dy
            first += 1
    assert first == total


def section_svg(section, heading, start):
    groups = sorted(
        (g for g in section["groups"] if g["total"]),
        key=lambda group: (-group["total"], group["name"].casefold()),
    )
    parts = [note(start + 3, 22,
                  f'{heading}: {section["percent"]}% ({section["done"]}/{section["total"]})', 15)]
    for group, x, y, w, h in partition(groups, start, TOP, PANEL_WIDTH, PANEL_HEIGHT):
        done = group["done_names"]
        partial = group["partial_names"]
        todo = group["todo_names"]
        sample = (done + partial + todo)[:22]
        hint = (f'{group["name"]} — {len(done)} verified, {len(partial)} partial, '
                f'{len(todo)} pending ({group["total"]} tracked)\n'
                + "\n".join(sample))
        if group["total"] > len(sample):
            hint += f'\n... and {group["total"] - len(sample)} others; see progress.html'
        parts.append(f'<g><title>{escape(hint)}</title>')
        colors = ([SUCCESS] * len(done) + [PARTIAL] * len(partial) +
                  [PENDING] * len(todo))
        for color, (px, py, pw, ph) in zip(
                colors, subcells(group["total"], x + 0.8, y + 0.8,
                                 max(0.1, w - 1.6), max(0.1, h - 1.6))):
            parts.append(tile(px, py, pw, ph, color))
        parts.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" '
                     f'height="{h:.2f}" fill="none" stroke="{DARK}" stroke-width="2"/>')
        maximum = max(0, int((w - 8) / 6.8))
        if maximum >= 4 and h > 21:
            label = group["label"][:maximum]
            parts.append(note(x + 4, y + min(h - 4, 15), label))
        parts.append("</g>")
    return parts


def make_map(state):
    canvas_height = TOP + PANEL_HEIGHT
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" '
             f'width="{WIDTH}" height="{canvas_height}" '
             f'viewBox="0 0 {WIDTH} {canvas_height}" role="img" '
             'aria-label="AnyiOS implementation progress by API family and runtime subsystem">',
             tile(0, 0, WIDTH, canvas_height, DARK, 0)]
    parts.extend(section_svg(state["libraries"], "iOS API exports*", 0))
    parts.extend(section_svg(state["runtime"], "Compatibility work gates*", PANEL_WIDTH + GAP))
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def make_badge(label, state):
    pct = state["percent"]
    color = ("#4c1" if pct >= 90 else "#97ca00" if pct >= 60 else
             "#dfb317" if pct >= 30 else "#fe7d37")
    value = f'{pct}%'
    left = 10 + 7 * len(label)
    right = 10 + 7 * len(value)
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{left + right}" '
        f'height="20" role="img" aria-label="{escape(label)}: {value}">'
        f'<rect width="{left}" height="20" fill="#555"/>'
        f'<rect x="{left}" width="{right}" height="20" fill="{color}"/>'
        '<g fill="#fff" font-family="Verdana,DejaVu Sans,sans-serif" '
        'font-size="11" text-anchor="middle">'
        f'<text x="{left/2}" y="14">{escape(label)}</text>'
        f'<text x="{left + right/2}" y="14">{value}</text>'
        '</g></svg>\n'
    )


def make_table(heading, section):
    rows = [f'<h2>{escape(heading)}</h2>', '<table>',
            '<thead><tr><th>Family / subsystem</th><th>Verified</th>'
            '<th>Partial</th><th>Pending</th><th>Total</th></tr></thead><tbody>']
    for group in sorted(section["groups"], key=lambda g: g["name"].casefold()):
        done = group["done_names"]
        partial = group["partial_names"]
        todo = group["todo_names"]
        rows.append(f'<tr><td><details><summary>{escape(group["name"])}</summary><ul>')
        for status, names in (("Verified", done), ("Partial", partial), ("Pending", todo)):
            for name in names:
                rows.append(f'<li>{escape(status)}: <code>{escape(name)}</code></li>')
        rows.append('</ul></details></td>'
                    f'<td>{len(done)}</td><td>{len(partial)}</td>'
                    f'<td>{len(todo)}</td><td>{group["total"]}</td></tr>')
    rows.append(f'</tbody><tfoot><tr><th>Total</th><th>{section["done"]}</th>'
                f'<th>{section["partial"]}</th><th>{section["pending"]}</th>'
                f'<th>{section["total"]}</th></tr></tfoot></table>')
    return "\n".join(rows)


def make_html(state):
    return "\n".join([
        '<!doctype html><html lang="en"><head><meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width, initial-scale=1">',
        '<title>AnyiOS compatibility coverage</title>',
        '<style>body{background:#0d1117;color:#e6edf3;max-width:1080px;margin:auto;'
        'font:14px system-ui;padding:18px}img{max-width:100%}table{width:100%;'
        'border-collapse:collapse}td,th{border:1px solid #30363d;padding:7px;'
        'vertical-align:top}td:not(:first-child){text-align:right}a{color:#79c0ff}'
        'details{max-height:240px;overflow:auto}code{font-size:12px}summary{cursor:pointer}'
        'li{padding:2px 0}</style></head><body>',
        '<h1>AnyiOS — compatibility progress</h1>',
        '<p><img src="badge-apis.svg" alt="Known API exports progress"> '
        '<img src="badge-runtime.svg" alt="Runtime subsystem progress"></p>',
        '<img src="progress.svg" alt="API and runtime capability treemap">',
        '<p><b>Scope:</b> only explicitly inventoried candidate API exports and '
        'compatibility gates. These are not complete Apple API inventories, and '
        'verified narrow functions do not imply app compatibility. '
        '<span style="color:#2ea043">■</span> verified, '
        '<span style="color:#d29922">■</span> partial, '
        '<span style="color:#6e7681">■</span> pending/unverified.</p>',
        make_table("iOS ABI/API export candidates", state["libraries"]),
        make_table("Runtime, frameworks and Windows compatibility gates", state["runtime"]),
        '<p>Source: <a href="' + SOURCE + '/tools/progress.py">progress generator</a>, '
        '<a href="' + SOURCE + '/tools/api_inventory.json">API inventory</a>, '
        '<a href="' + SOURCE + '/tools/compat_capabilities.json">capability catalog</a>, '
        '<a href="' + SOURCE + '/tools/api_manifest.json">API evidence</a>.</p>',
        '</body></html>\n',
    ])


def generate(root):
    state = collect(root)
    output = {
        "progress.json": json.dumps(state, indent=2, sort_keys=True) + "\n",
        "progress.svg": make_map(state),
        "badge-apis.svg": make_badge("iOS APIs*", state["libraries"]),
        "badge-runtime.svg": make_badge("runtime*", state["runtime"]),
        "progress.html": make_html(state),
    }
    return state, output


def names_by_status(section, key):
    return {(g["name"], name) for g in section["groups"] for name in g[key]}


def difference(old, new):
    lines = []
    for key, heading in (("libraries", "iOS API exports"),
                         ("runtime", "Runtime compatibility gates")):
        before, after = old[key], new[key]
        done_old = names_by_status(before, "done_names")
        done_new = names_by_status(after, "done_names")
        partial_old = names_by_status(before, "partial_names")
        partial_new = names_by_status(after, "partial_names")
        all_old = done_old | partial_old | names_by_status(before, "todo_names")
        all_new = done_new | partial_new | names_by_status(after, "todo_names")
        transitions = [
            ("Verified now", done_new - done_old),
            ("Newly partial", partial_new - partial_old),
            ("Regressed from verified", done_old - done_new),
            ("Added to inventory", all_new - all_old),
            ("Removed from inventory", all_old - all_new),
        ]
        lines.append(f'### {heading}: {before["percent"]}% → {after["percent"]}%')
        for name, items in transitions:
            if items:
                lines.append(f'\n<details><summary>{name}: {len(items)}</summary>\n')
                lines.append("| Family | Name |\n|---|---|")
                lines.extend(f'| {g} | {n} |' for g, n in sorted(items)[:100])
                if len(items) > 100:
                    lines.append(f'| … | and {len(items)-100} others |')
                lines.append("\n</details>\n")
        lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path("docs"))
    parser.add_argument("--root", type=Path, default=REPO)
    parser.add_argument("--check", action="store_true",
                        help="reject out-of-date committed SVG, badges, HTML and JSON")
    parser.add_argument("--compare", nargs=2, type=Path, metavar=("BEFORE", "AFTER"))
    args = parser.parse_args()
    if args.compare:
        old, new = [json.loads(path.read_text(encoding="utf-8")) for path in args.compare]
        print(difference(old, new))
        return
    state, assets = generate(args.root)
    target = args.output
    if not target.is_absolute():
        target = args.root / target
    if args.check:
        for name, body in assets.items():
            path = target / name
            if not path.is_file() or path.read_text(encoding="utf-8") != body:
                parser.error(f"Stale {path}: regenerate with python3 tools/progress.py")
    else:
        target.mkdir(parents=True, exist_ok=True)
        for name, body in assets.items():
            (target / name).write_text(body, encoding="utf-8")
    print(f'API exports: {state["libraries"]["done"]}/{state["libraries"]["total"]} '
          f'({state["libraries"]["percent"]}%), partial {state["libraries"]["partial"]}')
    print(f'Runtime: {state["runtime"]["done"]}/{state["runtime"]["total"]} '
          f'({state["runtime"]["percent"]}%), partial {state["runtime"]["partial"]}')


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        sys.exit(str(error))
