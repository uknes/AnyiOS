"""Static application exposure analysis must NEVER promote runtime success."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(ROOT / "tools"))
import app_coverage
from test_macho_gap import valid_macho


class AppCoverageTests(unittest.TestCase):
    def test_static_reports_are_not_execution_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle = Path(temp) / "Sample.app"
            bundle.mkdir()
            (bundle / "App").write_bytes(bytes(valid_macho()))
            candidates = {"schema": 1, "groups": [
                {"name": "ObjC", "symbols": ["_objc_msgSend", "_UIApplicationMain"]}]}
            gates = {"schema": 1, "groups": [
                {"name": "UI", "items": [{"name": "Visual window", "status": "pending"}]}]}
            manifest = {"symbols": {"_objc_msgSend": {"status": "not implemented"}}}
            report = app_coverage.make_report(bundle, candidates, gates, manifest)
            self.assertEqual(report["status"], "static-intake-only")
            self.assertEqual(report["api_inventory_total"], 2)
            self.assertEqual(report["static_candidate_import_count"], 1)
            self.assertEqual(report["api_inventory_symbols_absent_from_static_imports"], ["_UIApplicationMain"])
            self.assertEqual(report["missing_api_candidates_present"], ["_objc_msgSend"])
            self.assertEqual(report["runtime_gate_inventory_total"], 1)
            self.assertEqual(report["runtime_gates_exercised_by_app"], 0)
            self.assertFalse(report["execution_attempted"])
            self.assertFalse(report["windows_x64_window"])
            self.assertFalse(report["windows_arm64_window"])
            self.assertFalse(report["input_verified"])

    def test_two_real_app_targets_are_explicitly_scoped(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle = Path(temp) / "UIKitCatalog.app"
            bundle.mkdir()
            (bundle / "UIKitCatalog").write_bytes(bytes(valid_macho()))
            api = {"schema": 1, "groups": [{"name": "ObjC", "symbols": ["_objc_msgSend"]}]}
            gates = {"schema": 1, "groups": [{"name": "UIKit", "items": [
                {"name": "Guest UIKit interactive button", "status": "pending"}]}]}
            manifest = {"symbols": {}}
            report = app_coverage.make_report(bundle, api, gates, manifest,
                                               target="appium-uicatalog")
            self.assertEqual(report["target"], "appium-uicatalog")
            self.assertEqual(report["static_candidate_import_count"], 1)
            self.assertEqual(report["runtime_gates_exercised_by_app"], 0)
            self.assertFalse(report["windows_arm64_window"])
            self.assertFalse(report["windows_x64_window"])
            self.assertFalse(report["input_verified"])
            with self.assertRaisesRegex(ValueError, "unknown unmodified"):
                app_coverage.make_report(bundle, api, gates, manifest, target="fake-app")

    def test_unknown_import_retained_outside_candidate_set(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle = Path(temp) / "Other.app"
            bundle.mkdir()
            (bundle / "Program").write_bytes(bytes(valid_macho()))
            report = app_coverage.make_report(bundle,
                {"schema": 1, "groups": [{"name": "other", "symbols": []}]},
                {"schema": 1, "groups": []}, {"symbols": {}})
            self.assertIn("_objc_msgSend", report["imports_not_in_candidate_inventory"])
            self.assertEqual(report["runtime_gate_inventory_total"], 0)

    def test_refuse_symlink_and_oversized_input(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle = Path(temp) / "Sample.app"
            bundle.mkdir()
            (bundle / "Real").write_bytes(bytes(valid_macho()))
            (bundle / "Alias").symlink_to(bundle / "Real")
            with self.assertRaisesRegex(ValueError, "symlink"):
                app_coverage.app_binary_paths(bundle)
            with self.assertRaisesRegex(ValueError, "expected an ordinary"):
                app_coverage.app_binary_paths(bundle / "Real")

    def test_refuse_invalid_candidate_inventory(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            app_coverage.inventory_symbols({"schema": 1, "groups": [
                {"symbols": ["_foo", "_foo"]}]})

    def test_source_inventory_counts_unchanged(self):
        import json
        apis = json.loads((ROOT / "tools/api_inventory.json").read_text())
        gates = json.loads((ROOT / "tools/compat_capabilities.json").read_text())
        self.assertEqual(len(app_coverage.inventory_symbols(apis)), 269)
        self.assertEqual(len(app_coverage.runtime_gate_rows(gates)), 239)


if __name__ == "__main__":
    unittest.main()
