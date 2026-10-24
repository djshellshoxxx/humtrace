"""Portable path/topology tests; native Mach-O loading and signing run in macOS CI."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import sys

spec = importlib.util.spec_from_file_location(
    "package_macos", Path(__file__).resolve().parents[1] / "tools/package_macos.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


class MacBundle(unittest.TestCase):
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
            (framework / "Versions/Current").symlink_to("A")
            (framework / "QtCore").symlink_to("Versions/Current/QtCore")
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
            (framework / "escape").symlink_to(Path(temporary))
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
