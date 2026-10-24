"""Portable path/topology tests; native Mach-O loading and signing run in macOS CI."""
import importlib.util
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
import sys

spec = importlib.util.spec_from_file_location(
    "package_macos", Path(__file__).resolve().parents[1] / "tools/package_macos.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


class MacBundle(unittest.TestCase):
    def test_build_versions_keep_minimum_os_separate_from_sdk(self):
        modern = packaging.parse_load_commands("""Load command 8
          cmd LC_BUILD_VERSION
      cmdsize 32
     platform 1
        minos 15.0
          sdk 26.0
       ntools 1
         tool LD
      version 1200.0
""")
        self.assertEqual(modern["build_versions"], [{"platform": "MACOS", "minos": "15.0", "sdk": "26.0"}])
        self.assertEqual(packaging.validate_minimum_os(modern, "15.0", "app")["minos"], "15.0")
        legacy = packaging.parse_load_commands("""Load command 7
          cmd LC_VERSION_MIN_MACOSX
      cmdsize 16
      version 10.15
          sdk 15.0
""")
        self.assertEqual(legacy["build_versions"], [{"platform": "MACOS", "minos": "10.15", "sdk": "15.0"}])
        textual = packaging.parse_load_commands("cmd LC_BUILD_VERSION\nplatform MACOS\nminos 26.0\nsdk 26.1\n")
        self.assertEqual(packaging.validate_minimum_os(textual, "26.0", "QtCore")["platform"], "MACOS")

    def test_minimum_os_gate_rejects_unknown_platform_missing_metadata_and_exceeded_baseline(self):
        for info, message in [
            ({"build_versions": []}, "missing"),
            ({"build_versions": [{"platform": "IOS", "minos": "15.0", "sdk": "26.0"}]}, "non-macOS"),
            ({"build_versions": [{"platform": "MACOS", "minos": "26.0", "sdk": "26.0"}]}, "exceeds"),
            ({"build_versions": [{"platform": "MACOS", "minos": "15.0", "sdk": None}]}, "SDK"),
            ({"build_versions": [{"platform": "MACOS", "minos": "15.bad", "sdk": "26.0"}]}, "minimum OS"),
            ({"build_versions": [{"platform": "MACOS", "minos": "15.10", "sdk": "26.0"}]}, "exceeds"),
        ]:
            with self.subTest(info=info), self.assertRaisesRegex(RuntimeError, message):
                packaging.validate_minimum_os(info, "15.0", "fixture")

    def test_load_command_bound_rejects_an_oversized_inspection(self):
        with self.assertRaisesRegex(RuntimeError, "load-command count limit"):
            packaging.parse_load_commands("cmd LC_UUID\n" * (packaging.MAX_LOAD_COMMANDS + 1))

    def test_cli_requires_an_explicit_supported_baseline(self):
        result = subprocess.run([sys.executable, str(Path(packaging.__file__)),
                                 "--app", "Example.app", "--qt-prefix", "qt", "--brew-prefix", "brew"],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--minimum-os", result.stderr)

    def test_manifest_hashes_final_signed_images_and_records_standalone_cli(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            app = root / "Example.app"
            executable = app / "Contents/MacOS/Example"
            cli = root / "humtrace"
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"unsigned GUI topology fixture")
            cli.write_bytes(b"unsigned CLI topology fixture")
            with (app / "Contents/Info.plist").open("wb") as output:
                plistlib.dump({"CFBundleExecutable": "Example"}, output)
            unsigned_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
            record = {"imports": ["/usr/lib/libSystem.B.dylib"], "rpaths": [], "identity": None,
                      "build_versions": [{"platform": "MACOS", "minos": "15.0", "sdk": "26.0"}]}
            calls = []

            def native_fixture(command):
                calls.append(list(map(str, command)))
                self.assertFalse((root / "MACOS_DEPENDENCIES.json").exists(), "manifest must follow all signing/verification")
                if command[0] == "lipo":
                    self.assertEqual(command[-1], "-archs", "lipo input precedes its operation")
                    return "x86_64 arm64\n"
                if "--sign" in command:
                    with (app / "Contents/Info.plist").open("rb") as source:
                        self.assertEqual(plistlib.load(source)["LSMinimumSystemVersion"], "26.0")
                    target = Path(command[-1])
                    target = executable if target == app else target
                    target.write_bytes(target.read_bytes() + b":signed")
                return ""

            result = packaging.finalize_package(app, executable, [executable], "26.0", "arm64",
                                                lambda _: record, native_fixture, cli=cli)
            persisted = json.loads((root / "MACOS_DEPENDENCIES.json").read_text())
            self.assertEqual(result, persisted)
            images = {item["file"]: item for item in result["images"]}
            self.assertEqual(set(images), {"Example.app/Contents/MacOS/Example", "humtrace"})
            gui = images["Example.app/Contents/MacOS/Example"]
            self.assertNotEqual(gui["sha256"], unsigned_hash)
            self.assertEqual(gui["sha256"], hashlib.sha256(executable.read_bytes()).hexdigest())
            self.assertEqual(gui["minos"], "15.0")
            self.assertEqual(gui["sdk"], "26.0")
            self.assertEqual(gui["imports"], ["/usr/lib/libSystem.B.dylib"])
            self.assertEqual(gui["rpaths"], [])
            self.assertEqual(result["maximum_image_minimum_os"], "15.0", "SDK does not raise the runtime minimum")
            self.assertTrue(any(command[-1] == str(cli) and "--verify" in command for command in calls))

    def test_standalone_cli_architecture_mismatch_stops_before_signing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            app, cli = root / "Example.app", root / "humtrace"
            executable = app / "Contents/MacOS/Example"
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"GUI topology fixture")
            cli.write_bytes(b"CLI topology fixture")
            info = {"imports": [], "rpaths": [],
                    "build_versions": [{"platform": "MACOS", "minos": "15.0", "sdk": "26.0"}]}

            def native_fixture(command):
                self.assertEqual(command[0], "lipo", "failure must precede signing")
                return "x86_64" if Path(command[1]) == cli else "arm64"

            with self.assertRaisesRegex(RuntimeError, "missing native architecture arm64.*humtrace"):
                packaging.finalize_package(app, executable, [executable], "26.0", "arm64",
                                            lambda _: info, native_fixture, cli=cli)
            self.assertFalse((root / "MACOS_DEPENDENCIES.json").exists())

    def test_image_count_bound_rejects_before_native_commands(self):
        with self.assertRaisesRegex(RuntimeError, "count limit"):
            packaging.finalize_package(Path("/stage/Example.app"), Path("/stage/Example.app/Contents/MacOS/Example"),
                [Path(f"/stage/Example.app/Contents/Frameworks/lib{i}.dylib") for i in range(packaging.MAX_BINARIES + 1)],
                "26.0", "arm64", lambda _: self.fail("inspection after count bound"),
                lambda _: self.fail("native command after count bound"))

    def test_failed_native_command_keeps_its_diagnostic(self):
        with self.assertRaisesRegex(RuntimeError, "native fixture failure"):
            packaging.run_command([sys.executable, "-c", "print('native fixture failure'); raise SystemExit(7)"])

    def test_load_commands_separate_identity_imports_and_paths_with_spaces(self):
        info = packaging.parse_load_commands("""Load command 0
          cmd LC_ID_DYLIB
      cmdsize 80
         name @rpath/libexample.dylib (offset 24)
Load command 1
          cmd LC_LOAD_DYLIB
      cmdsize 80
         name /opt/Home Brew/lib/libbrotli.dylib (offset 24)
Load command 2
          cmd LC_RPATH
      cmdsize 48
         path @loader_path/../Frameworks (offset 12)
Load command 3
          cmd LC_LOAD_WEAK_DYLIB
      cmdsize 80
         name /System/Library/Frameworks/AppKit.framework/Versions/C/AppKit (offset 24)
""")
        self.assertEqual(info["identity"], "@rpath/libexample.dylib")
        self.assertEqual(info["imports"], ["/opt/Home Brew/lib/libbrotli.dylib",
                         "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit"])
        self.assertEqual(info["rpaths"], ["@loader_path/../Frameworks"])

    def test_only_required_platform_plugins_are_staged(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app, qt = root / "Hum Trace.app", root / "qt"
            for name in ("platforms/libqcocoa.dylib", "platforms/libqoffscreen.dylib",
                         "imageformats/libqpdf.dylib", "platforminputcontexts/libqtvirtualkeyboardplugin.dylib"):
                source = qt / "plugins" / name
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(name.encode())
            copied = packaging.stage_platform_plugins(app, qt)
            self.assertEqual([p.name for p in copied], ["libqcocoa.dylib", "libqoffscreen.dylib"])
            self.assertEqual(sorted(p.relative_to(app).as_posix() for p in app.rglob("*") if p.is_file()),
                             ["Contents/PlugIns/platforms/libqcocoa.dylib", "Contents/PlugIns/platforms/libqoffscreen.dylib"])
            command = packaging.deployment_command(app, qt, root / "brew", copied)
            self.assertIn("-no-plugins", command)
            self.assertIn("-no-codesign", command)
            self.assertIn("-libpath=" + str(root / "brew/lib"), command)
            self.assertIn("-executable=" + str(copied[1]), command)

    def test_dependency_audit_requires_internal_transitive_closure(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / "Example.app"
            executable = app / "Contents/MacOS/Example"
            library = app / "Contents/Frameworks/libbrotli.dylib"
            for path in (executable, library):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"topology fixture only")
            images = {executable: {"imports": ["@rpath/libbrotli.dylib"], "rpaths": ["@loader_path/../Frameworks"]},
                      library: {"imports": ["/usr/lib/libSystem.B.dylib"], "rpaths": []}}
            packaging.audit_dependencies(app, executable, list(images), images.__getitem__)
            images[library]["imports"].append("@rpath/libmissing.dylib")
            with self.assertRaisesRegex(RuntimeError, "libmissing"):
                packaging.audit_dependencies(app, executable, list(images), images.__getitem__)

    def test_homebrew_reference_is_rejected_even_if_it_exists_on_builder(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app = root / "Example.app"
            executable = app / "Contents/MacOS/Example"
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"topology fixture only")
            external = root / "brew/lib/libwebp.dylib"
            external.parent.mkdir(parents=True)
            external.write_bytes(b"external")
            with self.assertRaisesRegex(RuntimeError, "external dependency"):
                packaging.audit_dependencies(app, executable, [executable],
                    lambda _: {"imports": [str(external)], "rpaths": []})

    def test_framework_symlinks_remain_internal_and_sign_inside_out(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / "Example.app"
            framework = app / "Contents/Frameworks/QtCore.framework"
            binary = framework / "Versions/A/QtCore"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"topology fixture only")
            (framework / "Versions/Current").symlink_to("A", target_is_directory=True)
            # UCRT Python can use '/' even for WindowsPath. Pass a raw Win32
            # relative target: Path would normalize it back under that runtime.
            relative_target = "Versions/Current/QtCore"
            if os.name == "nt":
                relative_target = relative_target.replace("/", "\\")
            (framework / "QtCore").symlink_to(relative_target)
            self.assertTrue((framework / "QtCore").exists(),
                f"framework symlinks failed: Current={os.readlink(framework / 'Versions/Current')!r}, "
                f"QtCore={os.readlink(framework / 'QtCore')!r}")
            executable = app / "Contents/MacOS/Example"
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"topology fixture only")
            plugin = app / "Contents/PlugIns/platforms/libqcocoa.dylib"
            plugin.parent.mkdir(parents=True)
            plugin.write_bytes(b"topology fixture only")
            packaging.validate_symlinks(app)
            order = packaging.signing_order(app, [executable, binary, plugin])
            self.assertLess(order.index(binary), order.index(framework))
            self.assertLess(order.index(framework), order.index(app))
            self.assertLess(order.index(plugin), order.index(app))
            self.assertEqual(order[-1], app)
            (framework / "escape").symlink_to(Path(temporary), target_is_directory=True)
            with self.assertRaisesRegex(RuntimeError, "symlink escapes"):
                packaging.validate_symlinks(app)

    def test_relocation_removes_absolute_rpaths_and_adds_local_framework_path(self):
        app = Path("/stage/Example.app")
        plugin = app / "Contents/PlugIns/platforms/libqcocoa.dylib"
        command = packaging.relocation_command(app, plugin,
            {"imports": ["@rpath/QtGui.framework/Versions/A/QtGui"],
             "rpaths": ["/opt/homebrew/lib", "@loader_path/../../Frameworks"]})
        self.assertEqual(command, ["install_name_tool", "-delete_rpath", "/opt/homebrew/lib", str(plugin)])
        command = packaging.relocation_command(app, plugin,
            {"imports": ["@rpath/QtGui.framework/Versions/A/QtGui"], "rpaths": ["/opt/homebrew/lib"]})
        self.assertEqual(command, ["install_name_tool", "-delete_rpath", "/opt/homebrew/lib",
                                  "-add_rpath", "@loader_path/../../Frameworks", str(plugin)])


if __name__ == "__main__":
    unittest.main()
