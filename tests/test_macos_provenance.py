"""Tests for the bounded macOS Homebrew candidate inventory."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "package_macos_provenance", ROOT / "tools/package_macos_provenance.py")
provenance = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(provenance)


class HomebrewProvenance(unittest.TestCase):
    def fixture(self, root, *, duplicate=False):
        prefix = root / "brew"
        cellar = prefix / "Cellar"
        formulae = [
            {"name": "qtbase", "full_name": "qtbase", "tap": "homebrew/core",
             "license": "LGPL-3.0-only OR GPL-3.0-only", "homepage": "https://qt.io",
             "urls": {"stable": {"url": "https://example.test/qtbase.tar.xz",
                                  "checksum": "a" * 64}},
             "dependencies": ["openssl@3"], "installed": [{"version": "6.9.3"}]},
        ]
        for name, version in [("qtbase", "6.9.3")]:
            keg = cellar / name / version
            binary = keg / "lib/QtCore.framework/Versions/A/QtCore"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"source QtCore fixture")
            formula_file = keg / ".brew" / f"{name}.rb"
            formula_file.parent.mkdir()
            formula_file.write_text('class Qtbase < Formula; end\n', encoding="utf-8")
        if duplicate:
            keg = cellar / "qtbase-next" / "7.0.0"
            binary = keg / "lib/QtCore.framework/Versions/A/QtCore"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"second QtCore fixture")
            formulae.append({
                "name": "qtbase-next", "full_name": "example/tap/qtbase-next",
                "tap": "example/tap", "license": "BSD-3-Clause", "homepage": "https://example.test",
                "urls": {"stable": {"url": "https://example.test/qtbase-next.tar.gz",
                                    "checksum": "b" * 64}},
                "dependencies": [], "installed": [{"version": "7.0.0"}],
            })
        deps = root / "MACOS_DEPENDENCIES.json"
        deps.write_text(json.dumps({
            "schema_version": 1,
            "images": [
                {"file": "Example.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore",
                 "sha256": "c" * 64, "imports": []},
                {"file": "humtrace", "sha256": "d" * 64, "imports": []},
            ],
        }), encoding="utf-8")
        info = root / "brew-info.json"
        info.write_text(json.dumps(formulae), encoding="utf-8")
        return prefix, deps, info

    def test_unique_formula_match_records_license_source_and_formula_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix, deps, info = self.fixture(root)
            stage = root / "package/homebrew-provenance"
            result = provenance.build_inventory(deps, info, prefix, stage)
            images = {item["file"]: item for item in result["images"]}
            qtcore = images["Example.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore"]
            self.assertEqual(qtcore["match_status"], "unique_formula")
            match = qtcore["candidates"][0]
            self.assertEqual((match["formula"], match["version"]), ("qtbase", "6.9.3"))
            self.assertEqual(match["license_metadata"], "LGPL-3.0-only OR GPL-3.0-only")
            self.assertEqual(match["stable_source_url"], "https://example.test/qtbase.tar.xz")
            source = prefix / "Cellar/qtbase/6.9.3/lib/QtCore.framework/Versions/A/QtCore"
            self.assertEqual(match["source_sha256"], hashlib.sha256(source.read_bytes()).hexdigest())
            formula_file = stage / match["formula_file"]["path"]
            self.assertTrue(formula_file.is_file())
            self.assertEqual(match["formula_file"]["sha256"], hashlib.sha256(formula_file.read_bytes()).hexdigest())
            self.assertTrue((stage / "BREW_INFO.json").is_file())
            self.assertEqual(json.loads((stage / "MACOS_HOMEBREW_PROVENANCE.json").read_text()), result)

    def test_duplicate_basename_from_distinct_formulae_is_ambiguous(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix, deps, info = self.fixture(root, duplicate=True)
            result = provenance.build_inventory(deps, info, prefix, root / "stage")
            qtcore = result["images"][0]
            self.assertEqual(qtcore["match_status"], "ambiguous")
            self.assertEqual({item["formula"] for item in qtcore["candidates"]}, {"qtbase", "qtbase-next"})

    def test_unmatched_image_is_explicit_and_not_guessed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix, deps, info = self.fixture(root)
            result = provenance.build_inventory(deps, info, prefix, root / "stage")
            image = next(item for item in result["images"] if item["file"] == "humtrace")
            self.assertEqual(image["match_status"], "unmapped")
            self.assertEqual(image["candidates"], [])

    def test_manifest_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix, deps, info = self.fixture(root)
            data = json.loads(deps.read_text())
            data["images"][0]["file"] = "../outside.dylib"
            deps.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "unsafe or incomplete"):
                provenance.build_inventory(deps, info, prefix, root / "stage")
            self.assertFalse((root / "stage").exists())

    def test_stage_must_be_new(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix, deps, info = self.fixture(root)
            stage = root / "stage"
            stage.mkdir()
            with self.assertRaisesRegex(RuntimeError, "already exists"):
                provenance.build_inventory(deps, info, prefix, stage)

    def test_cellar_symlink_escape_is_not_assigned_to_formula(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix, deps, info = self.fixture(root)
            outside = root / "outside"
            outside.write_bytes(b"not in the formula keg")
            escaped = prefix / "Cellar/qtbase/6.9.3/lib/escaped.dylib"
            escaped.symlink_to(outside)
            data = json.loads(deps.read_text())
            data["images"][0]["file"] = "Example.app/Contents/Frameworks/escaped.dylib"
            deps.write_text(json.dumps(data), encoding="utf-8")
            result = provenance.build_inventory(deps, info, prefix, root / "stage")
            image = next(item for item in result["images"] if item["file"].endswith("escaped.dylib"))
            self.assertEqual(image["match_status"], "unmapped")

    def test_aggregate_source_hash_limit_is_recorded_without_discarding_owner(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix, deps, info = self.fixture(root)
            with mock.patch.object(provenance, "MAX_TOTAL_HASH_BYTES", 1):
                result = provenance.build_inventory(deps, info, prefix, root / "stage")
            image = next(item for item in result["images"] if item["file"] != "humtrace")
            self.assertEqual(image["match_status"], "unique_formula")
            self.assertIsNone(image["candidates"][0]["source_sha256"])
            self.assertEqual(image["candidates"][0]["source_hash_status"], "aggregate_limit")
            self.assertEqual(result["source_hash_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
