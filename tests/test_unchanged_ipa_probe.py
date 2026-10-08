"""An unchanged original iOS binary is read-only or explicitly refused."""
import hashlib
import json
from pathlib import Path
import plistlib
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import unchanged_ipa_probe as intake


def synthetic_macho(cryptid=0):
    header = struct.pack("<8I", 0xfeedfacf, 0x0100000c, 0, 2,
                         1, 24, 0, 0)
    encryption = struct.pack("<6I", 0x2c, 24, 0, 0, cryptid, 0)
    return header + encryption


class UnchangedOriginalAppTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = {"schema": 1, "symbols": {}}

    def write_bundle(self, binary, name="Game"):
        bundle = self.root / "Game.app"
        bundle.mkdir(exist_ok=True)
        (bundle / "Info.plist").write_bytes(plistlib.dumps({
            "CFBundleExecutable": name,
            "CFBundleIdentifier": "dev.test.unchanged",
        }))
        (bundle / name).write_bytes(binary)
        return bundle

    def test_unmodified_app_byte_integrity_and_static_inspection(self):
        original = synthetic_macho()
        bundle = self.write_bundle(original)
        result = intake.examine(bundle, self.manifest)
        self.assertEqual(result["binary_sha256"], hashlib.sha256(original).hexdigest())
        self.assertEqual((bundle / "Game").read_bytes(), original)
        self.assertEqual(result["status"], "static-imports-inspected-only")
        self.assertFalse(result["execution_attempted"])
        self.assertFalse(result["window_created"])
        self.assertFalse(result["encrypted"])

    def test_unmodified_ipa_archive_no_extraction(self):
        original = synthetic_macho()
        ipa = self.root / "Original.ipa"
        payload = plistlib.dumps({"CFBundleExecutable": "Game"})
        with zipfile.ZipFile(ipa, "w") as archive:
            archive.writestr("Payload/Game.app/Info.plist", payload)
            archive.writestr("Payload/Game.app/Game", original)
        baseline = hashlib.sha256(ipa.read_bytes()).digest()
        result = intake.examine(ipa, self.manifest)
        self.assertEqual(result["status"], "static-imports-inspected-only")
        self.assertEqual(hashlib.sha256(ipa.read_bytes()).digest(), baseline)
        self.assertFalse((self.root / "Payload").exists())

    def test_protected_game_fails_closed_with_original_intact(self):
        protected = synthetic_macho(cryptid=1)
        bundle = self.write_bundle(protected)
        result = intake.examine(bundle, self.manifest)
        self.assertEqual(result["status"], "blocked-protected-macho")
        self.assertEqual(result["encryption_commands"][0]["cryptid"], 1)
        self.assertNotIn("imports", result)
        self.assertFalse(result["execution_attempted"])
        self.assertEqual((bundle / "Game").read_bytes(), protected)

    def test_unsafe_binary_name_refused(self):
        with self.assertRaises(intake.IntakeError):
            intake.validate_bundle_name("../Game")
        with self.assertRaises(intake.IntakeError):
            intake.validate_bundle_name(r"x\..\Game")

    def test_invalid_macho_rejected(self):
        with self.assertRaises(intake.IntakeError):
            intake.encryption_info(b"not a game")

    def test_missing_game_does_not_conjure_fake_output(self):
        target = json.loads((Path(__file__).resolve().parents[1] /
            "compatibility/targets/sneaky-sasquatch.json").read_text())
        self.assertFalse(target["original_binary"]["obtained"])
        self.assertEqual(target["status"]["guest_boot"], "not executed")
        self.assertEqual(target["status"]["window"], "not created")


if __name__ == "__main__":
    unittest.main()
