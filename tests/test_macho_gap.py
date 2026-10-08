"""Owned synthetic Mach-O regression tests for bounded API gap extraction."""
import importlib.util
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("macho_gap", ROOT / "tools" / "macho_gap.py")
gap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gap)


def field(name):
    return name.encode("ascii").ljust(16, b"\0")


def valid_macho():
    data = bytearray(0x1000)
    struct.pack_into("<8I", data, 0, 0xfeedfacf, gap.ARM64, 0, 2, 2, 256, 0, 0)
    struct.pack_into("<II16sQQQQiiII", data, 32,
                     0x19, 232, field("__DATA"), 0x100000000, 0x1000,
                     0, 0x1000, 7, 7, 2, 0)
    struct.pack_into("<16s16sQQIIIIIIII", data, 104,
                     field("__objc_methname"), field("__DATA"),
                     0x100000400, 16, 0x400, 0, 0, 0, 0, 0, 0, 0)
    struct.pack_into("<16s16sQQIIIIIIII", data, 184,
                     field("__objc_selrefs"), field("__DATA"),
                     0x100000500, 8, 0x500, 3, 0, 0, 0, 0, 0, 0)
    struct.pack_into("<6I", data, 264, 2, 24, 0x600, 2, 0x800, 80)
    data[0x400:0x40d] = b"viewDidLoad\0\0"
    struct.pack_into("<Q", data, 0x500, 0x100000400)
    names = b"\0_objc_msgSend\0_OBJC_CLASS_$_AppDelegate\0"
    data[0x800:0x800 + len(names)] = names
    struct.pack_into("<IBBHQ", data, 0x600, 1, 1, 0, 0, 0)
    struct.pack_into("<IBBHQ", data, 0x610, 15, 1, 0, 0, 0)
    return data


class GapTests(unittest.TestCase):
    def test_undefined_symbols_classes_and_selectors(self):
        result = gap.analyze(bytes(valid_macho()), {"symbols": {}})
        self.assertEqual([x["name"] for x in result["imports"]],
                         ["_OBJC_CLASS_$_AppDelegate", "_objc_msgSend"])
        self.assertEqual(result["objc_classes_imported"], ["AppDelegate"])
        self.assertEqual(result["objc_selector_refs_resolved"], {"viewDidLoad": 1})
        self.assertEqual(result["objc_selector_refs_unresolved"], 0)
        self.assertEqual(result["imports"][0]["status"], "unknown")

    def test_encoded_selector_not_guessed(self):
        data = valid_macho()
        struct.pack_into("<Q", data, 0x500, 0x8000000000000)
        result = gap.analyze(bytes(data), {"symbols": {}})
        self.assertEqual(result["objc_selector_refs_unresolved"], 1)
        self.assertEqual(result["objc_selector_refs_resolved"], {})

    def test_rejects_out_of_bounds_segment(self):
        data = valid_macho()
        struct.pack_into("<Q", data, 32 + 48, 0xffffffffffffffff)
        with self.assertRaises(gap.InvalidMachO):
            gap.analyze(bytes(data), {})

    def test_rejects_invalid_symtab(self):
        data = valid_macho()
        struct.pack_into("<I", data, 264 + 12, 0xfffffffe)
        with self.assertRaises(gap.InvalidMachO):
            gap.analyze(bytes(data), {})

    def test_rejects_bad_header_and_fat_slice(self):
        with self.assertRaises(gap.InvalidMachO):
            gap.analyze(b"x", {})
        inner = valid_macho()
        fat = bytearray(4096 + len(inner))
        struct.pack_into(">II", fat, 0, 0xcafebabe, 1)
        struct.pack_into(">IIIII", fat, 8, gap.ARM64, 0, 4096, len(inner), 12)
        fat[4096:] = inner
        self.assertEqual(gap.analyze(bytes(fat), {})["filetype"], 2)

    def test_chained_import_symbols(self):
        data = bytearray(50)
        struct.pack_into("<7I", data, 0, 0, 28, 30, 34, 1, 1, 0)
        struct.pack_into("<I", data, 30, 0)
        data[34:40] = b"_puts\0"
        self.assertEqual(gap.decode_chained_imports(data, 0, len(data)), ["_puts"])

    def test_zero_import_chained_fixups_ending_at_payload_size(self):
        data = bytearray(28)
        struct.pack_into("<7I", data, 0, 0, 28, 28, 28, 0, 1, 0)
        self.assertEqual(gap.decode_chained_imports(data, 0, len(data)), [])
        bad = bytearray(data)
        struct.pack_into("<I", bad, 12, 29)
        with self.assertRaises(gap.InvalidMachO):
            gap.decode_chained_imports(bad, 0, len(bad))

    def test_manifest_scope_not_inferred(self):
        result = gap.analyze(bytes(valid_macho()), {
            "symbols": {"_objc_msgSend": {"status": "not implemented"}}})
        self.assertEqual(next(x for x in result["imports"]
                              if x["name"] == "_objc_msgSend")["status"],
                         "not implemented")


if __name__ == "__main__":
    unittest.main()
