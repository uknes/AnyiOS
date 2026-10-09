"""AnyPS5-style compatibility reporting contract, independently implemented."""
import copy
import json
import sys
from pathlib import Path
import unittest
from xml.etree import ElementTree

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import progress


class ProgressTests(unittest.TestCase):
    def test_inventory_is_large_and_evidence_scoped(self):
        state = progress.collect(progress.REPO)
        self.assertGreaterEqual(state["libraries"]["total"], 200)
        self.assertGreaterEqual(state["runtime"]["total"], 200)
        self.assertEqual(state["libraries"]["done"], 3)
        self.assertEqual(state["libraries"]["partial"], 1)
        self.assertEqual(state["runtime"]["done"], 6)
        self.assertEqual(state["runtime"]["partial"], 11)
        self.assertLess(state["libraries"]["percent"], 3)
        self.assertLess(state["runtime"]["percent"], 5)
        self.assertEqual(
            state["libraries"]["done"] + state["libraries"]["partial"] +
            state["libraries"]["pending"], state["libraries"]["total"])

    def test_rendered_assets_valid_and_reproducible(self):
        a, first = progress.generate(progress.REPO)
        b, second = progress.generate(progress.REPO)
        self.assertEqual(a, b)
        self.assertEqual(first, second)
        for path in ("progress.svg", "badge-apis.svg", "badge-runtime.svg"):
            root = ElementTree.fromstring(first[path])
            self.assertTrue(root.tag.endswith("svg"))
        self.assertIn("Runtime, frameworks and Windows compatibility gates", first["progress.html"])
        self.assertIn("Objective-C", first["progress.svg"])
        self.assertEqual(json.loads(first["progress.json"])["schema"], 2)

    def test_discovery_catalog_is_separate_from_implementation_counts(self):
        state, assets = progress.generate(progress.REPO)
        catalog = state["technology_catalog"]
        rows = [i for g in catalog["groups"] for i in g["items"]]
        self.assertEqual(len(rows), 405)
        self.assertTrue(all(i["status"] == "unassessed" for i in rows))
        self.assertEqual(state["runtime"]["total"], 575)
        self.assertEqual(state["runtime"]["done"], 6)
        self.assertEqual(state["libraries"]["total"], 269)
        self.assertIn("AppAttest", assets["progress.md"])
        self.assertIn("applicability unassessed", assets["progress.svg"])
        self.assertIn("Foundation Models", assets["apple-technologies.md"])
        # The appended discovery panel must fit inside the SVG viewBox.
        root = ElementTree.fromstring(assets["progress.svg"])
        height = float(root.attrib["height"])
        for rect in root.iter("{http://www.w3.org/2000/svg}rect"):
            self.assertLessEqual(float(rect.attrib.get("y", 0)) + float(rect.attrib["height"]), height + 0.02)

    def test_status_transitions_and_regressions(self):
        state = progress.collect(progress.REPO)
        newer = copy.deepcopy(state)
        group = newer["runtime"]["groups"][0]
        name = group["todo_names"].pop()
        group["done_names"].append(name)
        newer["runtime"]["done"] += 1
        newer["runtime"]["pending"] -= 1
        report = progress.difference(state, newer)
        self.assertIn("Verified now: 1", report)
        reversed_report = progress.difference(newer, state)
        self.assertIn("Regressed from verified: 1", reversed_report)

    def test_reject_unverified_as_implemented(self):
        import tempfile
        import shutil
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "tools").mkdir()
            for name in ("api_manifest.json", "api_inventory.json", "compat_capabilities.json"):
                shutil.copy2(progress.REPO / "tools" / name, root / "tools" / name)
            api = root / "tools" / "api_manifest.json"
            data = json.loads(api.read_text(encoding="utf-8"))
            data["symbols"]["_objc_msgSend"] = {"status": "implemented"}
            api.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "lacks its exact evidence scope"):
                progress.collect(root)


if __name__ == "__main__":
    unittest.main()
