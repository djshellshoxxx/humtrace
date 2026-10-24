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

    def test_inventories_nested_plugin_from_exact_plugin_root_and_copies_qtbase_spdx(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "ucrt64" / "bin"
            install_root = runtime.parent
            plugin_root = install_root / "share" / "qt6" / "plugins"
            stage = root / "stage"
            (runtime).mkdir(parents=True)
            stage.mkdir()
            plugin = b"platform plugin"
            (plugin_root / "platforms").mkdir(parents=True)
            (plugin_root / "platforms" / "qwindows.dll").write_bytes(plugin)
            (stage / "platforms").mkdir()
            (stage / "platforms" / "qwindows.dll").write_bytes(plugin)
            license_path = install_root / "share" / "licenses" / "qt6-base" / "LICENSE"
            license_path.parent.mkdir(parents=True)
            license_path.write_bytes(b"Qt license\n")
            sbom_path = install_root / "share" / "qt6" / "sbom" / "qtbase-6.8.3.spdx"
            sbom_path.parent.mkdir(parents=True)
            sbom_path.write_bytes(b"SPDXVersion: SPDX-2.3\n")
            manifest = stage / "DEPENDENCIES.json"
            manifest.write_text(json.dumps([{
                "file": "platforms/qwindows.dll", "imports": [],
                "sha256": hashlib.sha256(plugin).hexdigest(),
            }]), encoding="utf-8")

            def run(command):
                if command[:2] == ["pacman", "-Qqo"]:
                    self.assertEqual(command[2], "/ucrt64/share/qt6/plugins/platforms/qwindows.dll")
                    return "mingw-w64-ucrt-x86_64-qt6-base\n"
                if command[:2] == ["pacman", "-Q"]:
                    return "mingw-w64-ucrt-x86_64-qt6-base 6.8.3-2\n"
                if command[:2] == ["pacman", "-Ql"]:
                    owner = "mingw-w64-ucrt-x86_64-qt6-base"
                    return (f"{owner} /ucrt64/share/licenses/qt6-base/LICENSE\n"
                            f"{owner} /ucrt64/share/qt6/sbom/qtbase-6.8.3.spdx\n")
                self.fail(f"unexpected command: {command}")

            result = licenses.collect_package_licenses(
                manifest, runtime, stage, run=run, plugin_root=plugin_root)
            item = result["dependencies"][0]
            self.assertEqual(item["origin"], "/ucrt64/share/qt6/plugins/platforms/qwindows.dll")
            self.assertEqual(item["package"], "mingw-w64-ucrt-x86_64-qt6-base")
            self.assertEqual(item["version"], "6.8.3-2")
            self.assertEqual(item["license_files"][0]["origin"],
                             "/ucrt64/share/licenses/qt6-base/LICENSE")
            spdx_relative = "licenses/mingw-w64-ucrt-x86_64-qt6-base/share/qt6/sbom/qtbase-6.8.3.spdx"
            self.assertEqual((stage / spdx_relative).read_bytes(), b"SPDXVersion: SPDX-2.3\n")
            self.assertEqual(item["spdx_file"]["path"], spdx_relative)
            self.assertEqual(item["spdx_file"]["origin"],
                             "/ucrt64/share/qt6/sbom/qtbase-6.8.3.spdx")
            self.assertEqual(item["spdx_file"]["sha256"], hashlib.sha256(sbom_path.read_bytes()).hexdigest())
            self.assertIn("spdx", [notice["kind"] for notice in result["notices"]])

    def test_nested_plugin_fails_if_exact_installed_origin_is_absent(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime, stage = root / "ucrt64" / "bin", root / "stage"
            plugin_root = root / "ucrt64" / "share" / "qt6" / "plugins"
            runtime.mkdir(parents=True)
            stage.mkdir()
            (stage / "platforms").mkdir()
            (stage / "platforms" / "qwindows.dll").write_bytes(b"plugin")
            manifest = stage / "DEPENDENCIES.json"
            manifest.write_text(json.dumps([{"file": "platforms/qwindows.dll", "imports": []}]), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "runtime origin is missing"):
                licenses.collect_package_licenses(manifest, runtime, stage, run=lambda command: "",
                                                  plugin_root=plugin_root)

    def test_nested_plugin_fails_when_installed_origin_hash_differs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime, stage = root / "ucrt64" / "bin", root / "stage"
            plugin_root = root / "ucrt64" / "share" / "qt6" / "plugins"
            runtime.mkdir(parents=True)
            stage.mkdir()
            (stage / "platforms").mkdir()
            (stage / "platforms" / "qwindows.dll").write_bytes(b"staged")
            (plugin_root / "platforms").mkdir(parents=True)
            (plugin_root / "platforms" / "qwindows.dll").write_bytes(b"different")
            manifest = stage / "DEPENDENCIES.json"
            manifest.write_text(json.dumps([{"file": "platforms/qwindows.dll", "imports": []}]), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "does not match installed origin"):
                licenses.collect_package_licenses(manifest, runtime, stage, run=lambda command: "",
                                                  plugin_root=plugin_root)

    def test_nested_plugin_root_must_not_escape_runtime_prefix(self):
        with self.assertRaisesRegex(RuntimeError, "escapes package prefix"):
            licenses._plugin_pacman_path("platforms/qwindows.dll", Path("/ucrt64/share/plugins"),
                                         Path("/outside"), "/ucrt64")

    def test_queries_plugin_root_instead_of_scanning_install_prefix(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "ucrt64" / "bin"
            install_root = runtime.parent
            stage = root / "stage"
            runtime.mkdir(parents=True)
            stage.mkdir()
            (runtime / "qmake6.exe").write_bytes(b"fixture")
            plugin_root = install_root / "share" / "qt6" / "plugins"
            (plugin_root / "platforms").mkdir(parents=True)
            (plugin_root / "platforms" / "qwindows.dll").write_bytes(b"plugin")
            (stage / "platforms").mkdir()
            (stage / "platforms" / "qwindows.dll").write_bytes(b"plugin")
            (install_root / "share" / "licenses" / "pkg").mkdir(parents=True)
            manifest = stage / "DEPENDENCIES.json"
            manifest.write_text(json.dumps([{"file": "platforms/qwindows.dll", "imports": []}]), encoding="utf-8")
            calls = []

            def run(command):
                calls.append(command)
                if "qmake6" in command[0]:
                    self.assertEqual(command[-2:], ["-query", "QT_INSTALL_PLUGINS"])
                    return "/ucrt64/share/qt6/plugins\n"
                if command[:2] == ["pacman", "-Qqo"]:
                    return "pkg\n"
                if command[:2] == ["pacman", "-Q"]:
                    return "pkg 1.0-1\n"
                if command[:2] == ["pacman", "-Ql"]:
                    return "pkg /ucrt64/share/licenses/pkg/\n"
                self.fail(f"unexpected command: {command}")

            # Owner lookup is the only later call before package inventory setup.
            self.assertRaisesRegex(RuntimeError, "no installed license files",
                licenses.collect_package_licenses, manifest, runtime, stage, run,
                plugin_root=None)
            self.assertEqual(calls[0][1:], ["-query", "QT_INSTALL_PLUGINS"])
            self.assertTrue(str(plugin_root).startswith(str(install_root)))

    def test_qtbase_requires_one_installed_spdx_file(self):
        with self.assertRaisesRegex(RuntimeError, "expected one installed QtBase SPDX"):
            licenses._parse_qtbase_spdx_paths(
                "mingw-w64-ucrt-x86_64-qt6-base /ucrt64/share/licenses/qt6-base/LICENSE\n",
                "mingw-w64-ucrt-x86_64-qt6-base", "/ucrt64")


if __name__ == "__main__":
    unittest.main()
