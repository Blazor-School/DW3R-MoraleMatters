"""Integration checks against temporary copies; never execute the game or load a mod."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest


class CheckerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = Path(OPTIONS.game).read_bytes()
        cls.original_hash = hashlib.sha256(cls.original).hexdigest()
        assert cls.original_hash == "b57a2d4cbd01c9c5abd651fa42f43a4b8fdae88da6403106cd5f5c1bf4dade70"
        cls.reference = json.loads(Path(__file__).with_name("reference.json").read_text())
        cls.checker = str(Path(OPTIONS.checker).resolve())

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="MoraleMatters checker kiem tra ")
        self.root = Path(self.temp.name)
        self.game = self.root / "DW3CE_RE-Win64-Shipping.exe"
        self.output = self.root / "reports"
        self.game.write_bytes(self.original)

    def tearDown(self):
        self.temp.cleanup()

    def run_checker(self, path=None, checker=None, env=None):
        before = hashlib.sha256(self.game.read_bytes()).hexdigest()
        result = subprocess.run([checker or self.checker, str(path or self.game), "--output", str(self.output)], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30, env=env)
        self.assertEqual(hashlib.sha256(self.game.read_bytes()).hexdigest(), before, "Checker modified the input EXE")
        return result

    def report(self, path=None, checker=None, env=None):
        result = self.run_checker(path, checker, env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        files = sorted(self.output.glob("*.json"), key=lambda p: p.stat().st_mtime_ns)
        self.assertTrue(files)
        text = files[-1].read_text(encoding="utf-8")
        self.assertNotIn(str(self.root), text)
        self.assertNotIn(str(self.root).replace("\\", "\\\\"), text)
        report = json.loads(text)
        self.assertTrue(all("Expected" not in probe for probe in report["Actual"]["Probes"]))
        return report

    def mutate(self, offset, value=None):
        with self.game.open("r+b") as file:
            file.seek(offset)
            original = file.read(1)[0]
            file.seek(offset)
            file.write(bytes([original ^ 1 if value is None else value]))

    def test_supported_and_folder_detection(self):
        report = self.report(self.root)
        self.assertEqual(report["Verdict"], "KNOWN_SUPPORTED_EXE")
        self.assertEqual(len(report["Actual"]["Probes"]), 51)
        self.assertTrue(all(p["Match"] for p in report["Actual"]["Probes"]))
        self.assertTrue(all(s["HashMatch"] and s["LayoutMatch"] for s in report["SectionComparisons"]))

    def test_header_only_difference(self):
        # Unused DOS-header reserved field; the PE header remains unchanged.
        self.mutate(0x30)
        report = self.report()
        self.assertEqual(report["Verdict"], "SAME_SECTIONS_DIFFERENT_FILE_REVIEW_REQUIRED")
        self.assertNotEqual(report["Actual"]["OutsideSections"], self.reference["OutsideSections"])

    def test_appended_overlay(self):
        with self.game.open("ab") as file:
            file.write(b"checker diagnostic fixture")
        report = self.report()
        self.assertEqual(report["Verdict"], "SAME_SECTIONS_DIFFERENT_FILE_REVIEW_REQUIRED")
        self.assertEqual(report["Actual"]["FileSize"], len(self.original) + 26)

    def test_additional_helper_probe_and_changed_block(self):
        rva = 0x11F69B0
        section = next(s for s in self.reference["Sections"] if s["Rva"] <= rva < s["Rva"] + s["RawSize"])
        self.mutate(section["RawOffset"] + rva - section["Rva"])
        report = self.report()
        self.assertEqual(report["Verdict"], "DIFFERENT_BUILD_REVIEW_REQUIRED")
        failed = [p["Rva"] for p in report["Actual"]["Probes"] if not p["Match"]]
        self.assertEqual(failed, [rva])
        changed = [s for s in report["SectionComparisons"] if not s["HashMatch"]]
        self.assertEqual(len(changed), 1)
        block = changed[0]["ChangedBlocks"][0]
        self.assertEqual(block["SectionOffset"], ((rva - section["Rva"]) // 65536) * 65536)
        self.assertEqual(block["ActualFileOffset"], section["RawOffset"] + block["SectionOffset"])

    def test_code_outside_probes_still_rejected(self):
        section = next(s for s in self.reference["Sections"] if s["Name"] == ".text")
        self.mutate(section["RawOffset"] + 123)
        report = self.report()
        self.assertEqual(report["Verdict"], "DIFFERENT_BUILD_REVIEW_REQUIRED")
        self.assertTrue(all(p["Match"] for p in report["Actual"]["Probes"]))

    def test_read_only_data_difference(self):
        section = next(s for s in self.reference["Sections"] if s["Name"] == ".rdata")
        self.mutate(section["RawOffset"] + 100)
        report = self.report()
        self.assertEqual(report["Verdict"], "DIFFERENT_BUILD_REVIEW_REQUIRED")
        changed = [s["Name"] for s in report["SectionComparisons"] if not s["HashMatch"]]
        self.assertEqual(changed, [".rdata"])

    def test_layout_difference(self):
        pe_offset = struct.unpack_from("<I", self.original, 0x3C)[0]
        # OptionalHeader.AddressOfEntryPoint is 16 bytes into the optional header.
        self.mutate(pe_offset + 24 + 16)
        report = self.report()
        self.assertEqual(report["Verdict"], "DIFFERENT_BUILD_REVIEW_REQUIRED")
        self.assertIn("EntryPointRva", report["LayoutDifferences"])
        self.assertTrue(all(s["HashMatch"] for s in report["SectionComparisons"]))

    def test_invalid_and_truncated_files(self):
        for fixture in [b"Not a PE executable" * 8, self.original[:4096]]:
            with self.subTest(length=len(fixture)):
                self.game.write_bytes(fixture)
                result = self.run_checker()
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("could not complete", result.stderr)
                self.assertFalse(self.output.exists())

    def test_invalid_section_bounds(self):
        pe_offset = struct.unpack_from("<I", self.original, 0x3C)[0]
        optional_size = struct.unpack_from("<H", self.original, pe_offset + 20)[0]
        section_header = pe_offset + 24 + optional_size
        with self.game.open("r+b") as file:
            file.seek(section_header + 20)
            file.write(struct.pack("<I", len(self.original) - 2))
        result = self.run_checker()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertFalse(self.output.exists())

    def test_reports_do_not_overwrite(self):
        self.report()
        original_reports = {p: p.read_bytes() for p in self.output.iterdir()}
        self.report()
        self.assertEqual(len(list(self.output.iterdir())), 4)
        for file, contents in original_reports.items():
            self.assertEqual(file.read_bytes(), contents)

    def test_missing_input_and_wrong_launcher(self):
        for path in [self.root / "missing", self.root / "DW3CE_RE.exe"]:
            if path.suffix == ".exe":
                path.write_bytes(b"not the shipping EXE")
            result = self.run_checker(path)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertFalse(self.output.exists())

    def test_steam_build_and_no_account_data(self):
        game_root = self.root / "steamapps" / "common" / "DW3 Unicode kiem tra"
        win64 = game_root / "DW3CE_RE" / "Binaries" / "Win64"
        win64.mkdir(parents=True)
        shutil.move(self.game, win64 / self.game.name)
        self.game = win64 / self.game.name
        manifest = self.root / "steamapps" / "appmanifest_123.acf"
        manifest.write_text('"AppState" { "appid" "123" "buildid" "456" "installdir" "DW3 Unicode kiem tra" "LastOwner" "PRIVATE_ACCOUNT_ID" }')
        report = self.report(game_root)
        self.assertEqual(report["Steam"], {"AppId": "123", "BuildId": "456"})
        self.assertNotIn("PRIVATE_ACCOUNT_ID", json.dumps(report))

    def test_self_contained_no_dotnet_on_path(self):
        # Move only the EXE: no sidecar DLLs or reference file are required.
        portable = self.root / "portable" / Path(self.checker).name
        portable.parent.mkdir()
        shutil.copy2(self.checker, portable)
        env = dict(os.environ)
        env["PATH"] = str(Path(os.environ["SystemRoot"]) / "System32")
        env["DOTNET_ROOT"] = str(self.root / "no-dotnet-here")
        report = self.report(checker=str(portable), env=env)
        self.assertEqual(report["Verdict"], "KNOWN_SUPPORTED_EXE")

    def test_reference_generation_refuses_unknown_exe(self):
        self.mutate(0x30)
        reference_output = self.root / "new-reference.json"
        result = subprocess.run([self.checker, "--create-reference", str(self.game), "--source-root", str(Path(__file__).parent.parent), "--output", str(reference_output)], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertFalse(reference_output.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--checker", required=True)
    parser.add_argument("--game", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining], verbosity=2)
