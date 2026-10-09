#!/usr/bin/env python3
"""Read-only ARM64 iOS .app/static API exposure report.

This tool inspects original binaries without editing their bytes. An import is
not a verified implementation or observed call, and an API inventory match is
NOT a runtime gate. Never generate a success claim for Windows guest graphics.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import macho_gap

MAX_FILES = 25000
MAX_MACHOS = 256
MAX_BINARY = 128 * 1024 * 1024
MACHO64_LE = b"\xcf\xfa\xed\xfe"
FAT_BE = (b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf")


def app_binary_paths(bundle: Path):
    if not bundle.is_dir() or bundle.is_symlink() or bundle.suffix != ".app":
        raise ValueError("expected an ordinary unmodified .app directory")
    count = 0
    hits = []
    for path in sorted(bundle.rglob("*")):
        count += 1
        if count > MAX_FILES:
            raise ValueError("app bundle contains too many entries")
        if path.is_symlink():
            raise ValueError("symlinked bundle input is not supported")
        if not path.is_file():
            continue
        with path.open("rb") as source:
            magic = source.read(4)
        if magic not in (MACHO64_LE, *FAT_BE):
            continue
        size = path.stat().st_size
        if size > MAX_BINARY:
            raise ValueError("Mach-O exceeds 128 MiB safe inspection bound")
        hits.append(path)
        if len(hits) > MAX_MACHOS:
            raise ValueError("too many Mach-O binaries in app")
    return hits


def inventory_symbols(data):
    if data.get("schema") != 1 or not isinstance(data.get("groups"), list):
        raise ValueError("invalid API candidate inventory")
    names = []
    for group in data["groups"]:
        names.extend(group["symbols"])
    if len(set(names)) != len(names):
        raise ValueError("duplicate API candidate")
    return set(names)


def runtime_gate_rows(data):
    if data.get("schema") != 1 or not isinstance(data.get("groups"), list):
        raise ValueError("invalid runtime capability inventory")
    result = []
    for group in data["groups"]:
        for gate in group["items"]:
            result.append({"group": group["name"], "name": gate["name"],
                           "repository_status": gate["status"],
                           "app_executed": False, "app_tested": False})
    return result


def make_report(bundle, api_candidates, gate_inventory, manifest, binary_paths=None, *, target="wikipedia-ios"):
    if target not in ("wikipedia-ios", "appium-uicatalog"):
        raise ValueError("unknown unmodified original iOS coverage target")
    candidates = inventory_symbols(api_candidates)
    gate_rows = runtime_gate_rows(gate_inventory)
    paths = binary_paths if binary_paths is not None else app_binary_paths(bundle)
    report = {
        "schema": 1, "target": target,
        "original_sources_edited": False, "binary_modified": False,
        "execution_attempted": False, "windows_x64_window": False,
        "windows_arm64_window": False, "input_verified": False,
        "api_inventory_total": len(candidates),
        "runtime_gate_inventory_total": len(gate_rows),
        "runtime_gates_exercised_by_app": 0,
        "runtime_gates_exercised_note": "Not measured; static Mach-O symbols cannot prove runtime gate execution.",
        "binaries": [], "imports_not_in_candidate_inventory": [],
        "missing_api_candidates_present": [],
        "status": "static-intake-only",
    }
    observed = set()
    imports_outside = set()
    for path in paths:
        data = path.read_bytes()
        item = {"path": str(path.relative_to(bundle)) if path.is_relative_to(bundle) else path.name,
                "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
        try:
            inspected = macho_gap.analyze(data, manifest)
            names = {entry["name"] for entry in inspected["imports"]}
            observed.update(names & candidates)
            imports_outside.update(names - candidates)
            item.update({"status": "static-inspected", "filetype": inspected["filetype"],
                         "import_count": len(names),
                         "framework_dependencies": inspected["dependencies"],
                         "import_symbols": sorted(names),
                         "objc_selector_refs_unresolved": inspected["objc_selector_refs_unresolved"]})
        except (macho_gap.InvalidMachO, ValueError) as error:
            item.update({"status": "blocked-unsupported-macho",
                         "blocker": str(error)[:512]})
        report["binaries"].append(item)
    report["api_inventory_symbols_present_in_static_imports"] = sorted(observed)
    report["api_inventory_symbols_absent_from_static_imports"] = sorted(candidates - observed)
    report["static_candidate_import_count"] = len(observed)
    report["imports_not_in_candidate_inventory"] = sorted(imports_outside)
    report["static_outside_inventory_count"] = len(imports_outside)
    report["missing_api_candidates_present"] = sorted(
        name for name in observed
        if manifest.get("symbols", {}).get(name, {}).get("status") != "implemented")
    report["runtime_gate_manifest"] = gate_rows
    if not paths:
        report["status"] = "blocked-no-arm64-macho"
    elif any(b["status"] != "static-inspected" for b in report["binaries"]):
        report["status"] = "partial-unsupported-macho"
    return report


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--target", choices=("wikipedia-ios", "appium-uicatalog"),
                        default="wikipedia-ios")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        api = json.loads((root / "tools/api_inventory.json").read_text("utf-8"))
        gates = json.loads((root / "tools/compat_capabilities.json").read_text("utf-8"))
        manifest = json.loads((root / "tools/api_manifest.json").read_text("utf-8"))
        result = make_report(args.bundle, api, gates, manifest, target=args.target)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, sort_keys=True, indent=2) + "\n",
                               encoding="utf-8")
        print(json.dumps({k: v for k, v in result.items()
                          if k in ("status", "api_inventory_total", "runtime_gate_inventory_total",
                                   "static_candidate_import_count", "static_outside_inventory_count",
                                   "runtime_gates_exercised_by_app")}, indent=2))
        return 0 if result["status"] == "static-intake-only" else 2
    except (OSError, ValueError) as error:
        print("app-coverage: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
