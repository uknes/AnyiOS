#!/usr/bin/env python3
"""Generate a deterministic, evidence-scoped SVG API progress card.

Only the checked-in, explicitly tracked guest API symbols are counted.
Never infer game compatibility or full Apple framework coverage.
"""
import argparse
import html
import json
from pathlib import Path

GROUPS = {
    "Darwin C / process": ["_malloc", "_write", "_exit"],
    "Thread-local storage": ["__tlv_bootstrap"],
    "Objective-C runtime": ["_objc_msgSend", "_objc_autoreleasePoolPush",
                            "_objc_autoreleasePoolPop", "_NSStringFromClass"],
    "UIKit entry": ["_UIApplicationMain"],
}
COLORS = {"implemented": "#47d7a0", "partial": "#f5c86a",
          "not implemented": "#56637c", "unknown": "#56637c"}


def render(manifest):
    symbols = manifest["symbols"]
    tracked = [s for names in GROUPS.values() for s in names]
    if len(set(tracked)) != len(tracked) or set(tracked) != set(symbols):
        raise ValueError("Progress scope must explicitly classify every tracked manifest symbol")
    for key, item in symbols.items():
        if item.get("status") not in COLORS:
            raise ValueError("Unknown status for " + key)
        if item["status"] in ("implemented", "partial") and not item.get("scope"):
            raise ValueError("Verified/partial API needs scope evidence: " + key)
    totals = {key: sum(symbols[s]["status"] == key for s in tracked)
              for key in COLORS}
    def badge(label, value, color):
        left, right = 132, 82
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{left+right}" height="28" '
                f'viewBox="0 0 {left+right} 28" role="img" aria-label="{label}: {value}">'
                f'<rect width="{left+right}" height="28" rx="5" fill="#303a4c"/>'
                f'<path d="M{left} 0H{left+right-5}Q{left+right} 0 {left+right} 5V23'
                f'Q{left+right} 28 {left+right-5} 28H{left}Z" fill="{color}"/>'
                f'<text x="{left/2}" y="19" text-anchor="middle" fill="white" '
                f'font-family="Verdana,Arial,sans-serif" font-size="12">{label}</text>'
                f'<text x="{left+right/2}" y="19" text-anchor="middle" fill="#0b1220" '
                f'font-weight="bold" font-family="Verdana,Arial,sans-serif" font-size="12">{value}</text>'
                '</svg>\\n')
    implemented = totals["implemented"]
    partial = totals["partial"]
    groups = [("Darwin / process", GROUPS["Darwin C / process"]),
              ("TLS", GROUPS["Thread-local storage"]),
              ("Objective-C", GROUPS["Objective-C runtime"]),
              ("UIKit", GROUPS["UIKit entry"])]
    output = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="850" height="330" viewBox="0 0 850 330" role="img" aria-label="AnyiOS API implementation map by subsystem">',
        '<rect width="850" height="330" rx="12" fill="#111827"/>',
        '<text x="24" y="36" fill="#f9fafb" font-family="Verdana,Arial,sans-serif" font-size="22" font-weight="bold">API implementation map</text>',
        '<text x="24" y="59" fill="#9ca3af" font-family="Verdana,Arial,sans-serif" font-size="12">Tracked guest ABI symbols only · green implemented · amber partial · slate missing</text>',
    ]
    colors = {"implemented": "#36c98c", "partial": "#eebd57", "not implemented": "#344256"}
    y = 86
    for label, names in groups:
        output.append(f'<text x="24" y="{y+16}" fill="#d1d5db" font-family="Verdana,Arial,sans-serif" font-size="14">{label}</text>')
        x = 180
        for name in names:
            w = max(100, min(170, 30 + len(name) * 6))
            status = symbols[name]["status"]
            color = colors[status]
            foreground = "#cbd5e1" if status == "not implemented" else "#111827"
            output.append(f'<rect x="{x}" y="{y}" width="{w}" height="28" rx="5" fill="{color}"/>')
            output.append(f'<text x="{x+w/2}" y="{y+18}" text-anchor="middle" fill="{foreground}" font-family="Consolas,monospace" font-size="11">{html.escape(name)}</text>')
            x += w + 8
        y += 55
    output.extend([
        '<text x="24" y="315" fill="#94a3b8" font-family="Verdana,Arial,sans-serif" font-size="11">3 implemented / 1 partial / 5 missing. Not a percentage of all iOS APIs or runnable apps.</text>',
        '</svg>',
    ])
    svg = "\\n".join(output) + "\\n"
    badges = {
        "docs/badge-apis.svg": badge("Guest APIs", f"{round(implemented / len(tracked) * 100)}%", "#55d4a0"),
        "docs/badge-runtime.svg": badge("Runtime partial", f"{partial}/{len(tracked)}", "#f0c36a"),
    }
    return svg, badges, {
        "schema": 1, "source": "tools/api_manifest.json",
        "scope": "tracked guest ABI symbols only",
        "implemented": implemented, "partial": partial, "not_implemented_or_unknown": remaining,
        "tracked": len(tracked),
        "categories": {
            name: {"implemented": sum(symbols[s]["status"] == "implemented" for s in names),
                   "partial": sum(symbols[s]["status"] == "partial" for s in names),
                   "tracked": len(names)}
            for name, names in GROUPS.items()
        },
        "disclaimer": "Not a percentage of iOS apps, Apple frameworks, or total Darwin APIs."
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=Path("tools/api_manifest.json"))
    parser.add_argument("--svg", type=Path, default=Path("docs/progress.svg"))
    parser.add_argument("--json", type=Path, default=Path("docs/progress.json"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    svg, badges, stats = render(json.loads(args.manifest.read_text(encoding="utf-8")))
    generated = {args.svg: svg, args.json: json.dumps(stats, indent=2, sort_keys=True) + "\n"}\n    generated.update({Path(name): content for name, content in badges.items()})
    if args.check:
        for path, content in generated.items():
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                parser.error(str(path) + " is stale; run python3 tools/progress.py")
    else:
        for path, content in generated.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    main()
