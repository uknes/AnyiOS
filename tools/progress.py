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
    width, height = 840, 390
    output = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-label="AnyiOS tracked guest API implementation progress">',
        '<rect width="840" height="390" rx="20" fill="#101726"/>',
        '<text x="32" y="43" fill="#f5f7fb" font-family="Arial,sans-serif" font-size="25" font-weight="bold">AnyiOS / Guest API coverage</text>',
        '<text x="32" y="68" fill="#9eacc2" font-family="Arial,sans-serif" font-size="13">Evidence-scoped manifest • NOT total iOS compatibility</text>',
    ]
    implemented = totals["implemented"]
    partial = totals["partial"]
    remaining = len(tracked) - implemented - partial
    output.append(f'<text x="32" y="115" fill="#f5f7fb" font-family="Arial,sans-serif" font-size="29" font-weight="bold">{implemented}/{len(tracked)} <tspan font-size="15" font-weight="normal" fill="#9eacc2">implemented ({implemented/len(tracked):.0%})</tspan></text>')
    output.append(f'<text x="32" y="137" fill="#9eacc2" font-family="Arial,sans-serif" font-size="13">{partial} partial • {remaining} unimplemented/unknown • denominator grows as APIs are inventoried</text>')
    def bar(x, y, w, h, names):
        output.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="#2a3449"/>')
        cursor = x
        for status in ("implemented", "partial"):
            count = sum(symbols[s]["status"] == status for s in names)
            segment = w * count / len(names)
            if segment:
                output.append(f'<rect x="{cursor:.1f}" y="{y}" width="{segment:.1f}" height="{h}" fill="{COLORS[status]}"/>')
                cursor += segment
    bar(32, 151, 776, 16, tracked)
    y = 210
    for group, names in GROUPS.items():
        complete = sum(symbols[s]["status"] == "implemented" for s in names)
        halfway = sum(symbols[s]["status"] == "partial" for s in names)
        output.append(f'<text x="32" y="{y}" fill="#e9effa" font-family="Arial,sans-serif" font-size="15">{html.escape(group)}</text>')
        output.append(f'<text x="808" y="{y}" fill="#b7c3d8" text-anchor="end" font-family="Arial,sans-serif" font-size="14">{complete}/{len(names)} verified' + (f' + {halfway} partial' if halfway else '') + '</text>')
        bar(32, y + 9, 776, 11, names)
        y += 44
    output.append('<text x="32" y="376" fill="#8998b0" font-family="Arial,sans-serif" font-size="12">Counts only tools/api_manifest.json. An import shim does not imply UIKit, SpriteKit, or app compatibility.</text>')
    output.append('</svg>')
    return "\n".join(output) + "\n", {
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
    svg, stats = render(json.loads(args.manifest.read_text(encoding="utf-8")))
    generated = {args.svg: svg, args.json: json.dumps(stats, indent=2, sort_keys=True) + "\n"}
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
