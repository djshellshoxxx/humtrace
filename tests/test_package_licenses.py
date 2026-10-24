"""Package-owner and license-file inventory tests without a pacman install."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "package_licenses", Path(__file__).resolve().parents[1] / "tools/package_licenses.py")
licenses = importlib.util.module_from_spec(spec)
spec.loader.exec_module(licenses)


class PackageLicenseInventory(unittest.TestCase):
    def test_copies_owned_package_license_and_records_origins_versions_and_hashes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "ucrt64" / "bin"
            install_root = runtime.parent
            stage = root / "stage"
            runtime.mkdir(parents=True)
            stage.mkdir()
            (runtime / "libqt.dll").write_bytes(b"runtime dll")
            (stage / "libqt.dll").write_bytes(b"runtime dll")
            dependency_manifest = stage / "DEPENDENCIES.json"
            dependency_manifest.write_text(json.dumps([
                {"file": "humtrace.exe", "imports": ["libqt.dll"], "sha256": "exe"},
                {"file": "libqt.dll", "imports": [], "sha256": hashlib.sha256(b"runtime dll").hexdigest()},
            ]), encoding="utf-8")
            license_file = install_root / "share" / "licenses" / "qt-base" / "LICENSE"
            license_file.parent.mkdir(parents=True)
            # Hash assertions must see the same fixture bytes on POSIX and Windows.
            license_file.write_bytes(b"license text\n")

            def run(command):
                if command[:2] == ["pacman", "-Qqo"]:
                    self.assertEqual(command[2], "/ucrt64/bin/libqt.dll")
                    return "qt-base\n"
                if command[:2] == ["pacman", "-Q"]:
                    return "qt-base 6.8.3-1\n"
                if command[:2] == ["pacman", "-Ql"]:
                    return ("qt-base /ucrt64/share/licenses/qt-base/\n"
                            "qt-base /ucrt64/share/licenses/qt-base/LICENSE\n"
                            "qt-base /ucrt64/share/qt/plugins/qwindows.dll\n")
                self.fail(f"unexpected command: {command}")

            result = licenses.collect_package_licenses(
                dependency_manifest, runtime, stage, run=run)

            copied = stage / "licenses" / "qt-base" / "LICENSE"
            self.assertEqual(copied.read_text(encoding="utf-8"), "license text\n")
            self.assertEqual(result["dependencies"][0]["file"], "libqt.dll")
            item = result["dependencies"][0]
            self.assertEqual(item["origin"], "/ucrt64/bin/libqt.dll")
            self.assertEqual(item["package"], "qt-base")
            self.assertEqual(item["version"], "6.8.3-1")
            self.assertEqual(item["license_files"][0]["path"], "licenses/qt-base/LICENSE")
            self.assertEqual(item["license_files"][0]["origin"],
                             "/ucrt64/share/licenses/qt-base/LICENSE")
            self.assertEqual(item["license_files"][0]["sha256"],
                             hashlib.sha256(b"license text\n").hexdigest())
            written = json.loads((stage / "DEPENDENCY_LICENSES.json").read_text(encoding="utf-8"))
            self.assertEqual(written, result)

    def test_fails_if_bundled_runtime_dependency_has_no_owner(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "ucrt64" / "bin"
            stage = root / "stage"
            runtime.mkdir(parents=True)
            stage.mkdir()
            (runtime / "missing.dll").write_bytes(b"dll")
            (stage / "missing.dll").write_bytes(b"dll")
            manifest = stage / "DEPENDENCIES.json"
            manifest.write_text(json.dumps([{"file": "missing.dll", "imports": []}]), encoding="utf-8")

            def run(command):
                if command[:2] == ["pacman", "-Qqo"]:
                    raise RuntimeError("no package owns /ucrt64/bin/missing.dll")
                self.fail(f"unexpected command: {command}")

            with self.assertRaisesRegex(RuntimeError, "no package owns"):
                licenses.collect_package_licenses(manifest, runtime, stage, run=run)
            self.assertFalse((stage / "DEPENDENCY_LICENSES.json").exists())

    def test_fails_when_owner_has_no_installed_license_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "ucrt64" / "bin"
            install_root = runtime.parent
            stage = root / "stage"
            runtime.mkdir(parents=True)
            stage.mkdir()
            (runtime / "runtime.dll").write_bytes(b"dll")
            (stage / "runtime.dll").write_bytes(b"dll")
            manifest = stage / "DEPENDENCIES.json"
            manifest.write_text(json.dumps([{"file": "runtime.dll", "imports": []}]), encoding="utf-8")
            (install_root / "share" / "licenses" / "pkg").mkdir(parents=True)

            def run(command):
                if command[:2] == ["pacman", "-Qqo"]:
                    return "pkg\n"
                if command[:2] == ["pacman", "-Q"]:
                    return "pkg 1.0-1\n"
                if command[:2] == ["pacman", "-Ql"]:
                    return "pkg /ucrt64/share/licenses/pkg/\n"
                self.fail(f"unexpected command: {command}")

            with self.assertRaisesRegex(RuntimeError, "no installed license files"):
                licenses.collect_package_licenses(manifest, runtime, stage, run=run)
            self.assertFalse((stage / "DEPENDENCY_LICENSES.json").exists())

    def test_copies_package_owned_license_outside_share_licenses(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "ucrt64" / "bin"
            install_root = runtime.parent
            stage = root / "stage"
            runtime.mkdir(parents=True)
            stage.mkdir()
            (runtime / "icu.dll").write_bytes(b"icu runtime")
            (stage / "icu.dll").write_bytes(b"icu runtime")
            manifest = stage / "DEPENDENCIES.json"
            manifest.write_text(json.dumps([
                {"file": "icu.dll", "imports": [],
                 "sha256": hashlib.sha256(b"icu runtime").hexdigest()},
            ]), encoding="utf-8")
            license_file = install_root / "share" / "icu" / "78.3" / "LICENSE"
            license_file.parent.mkdir(parents=True)
            license_file.write_bytes(b"ICU license\n")

            def run(command):
                if command[:2] == ["pacman", "-Qqo"]:
                    self.assertEqual(command[2], "/ucrt64/bin/icu.dll")
                    return "mingw-w64-ucrt-x86_64-icu\n"
                if command[:2] == ["pacman", "-Q"]:
                    return "mingw-w64-ucrt-x86_64-icu 78.3-4\n"
                if command[:2] == ["pacman", "-Ql"]:
                    return ("mingw-w64-ucrt-x86_64-icu /ucrt64/share/icu/78.3/\n"
                            "mingw-w64-ucrt-x86_64-icu /ucrt64/share/icu/78.3/LICENSE\n"
                            "mingw-w64-ucrt-x86_64-icu /ucrt64/bin/libicudt78.dll\n")
                self.fail(f"unexpected command: {command}")

            result = licenses.collect_package_licenses(manifest, runtime, stage, run=run)
            item = result["dependencies"][0]
            copied = stage / "licenses" / "mingw-w64-ucrt-x86_64-icu" / "share" / "icu" / "78.3" / "LICENSE"
            self.assertEqual(copied.read_bytes(), b"ICU license\n")
            self.assertEqual(item["version"], "78.3-4")
            self.assertEqual(item["license_files"], [{
                "path": "licenses/mingw-w64-ucrt-x86_64-icu/share/icu/78.3/LICENSE",
                "origin": "/ucrt64/share/icu/78.3/LICENSE",
                "sha256": hashlib.sha256(b"ICU license\n").hexdigest(),
            }])

    def test_rejects_license_candidate_outside_declared_prefix(self):
        with self.assertRaisesRegex(RuntimeError, "outside package prefix"):
            licenses._parse_license_paths(
                "pkg /other/share/licenses/pkg/LICENSE\n", "pkg", "/ucrt64")

    def test_rejects_license_paths_outside_the_package_license_tree(self):
        with self.assertRaisesRegex(RuntimeError, "unsafe pacman license path"):
            licenses._parse_license_paths(
                "pkg /ucrt64/share/licenses/pkg/../../bin/other.dll\n", "pkg", "/ucrt64")


if __name__ == "__main__":
    unittest.main()
