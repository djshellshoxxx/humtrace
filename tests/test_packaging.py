"""Packaging invariants; native PE execution is tested separately in CI."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "package_windows", Path(__file__).resolve().parents[1] / "tools/package_windows.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


class RuntimeClosure(unittest.TestCase):
    def test_transitive_dlls_are_copied_with_case_insensitive_imports(self):
        with tempfile.TemporaryDirectory() as root:
            stage, runtime, system = [Path(root) / p for p in ("stage", "runtime", "system")]
            for directory in (stage, runtime, system):
                directory.mkdir()
            (stage / "humtrace.exe").write_bytes(b"application")
            (runtime / "Qt6Core.dll").write_bytes(b"qt")
            (runtime / "libgcc.dll").write_bytes(b"gcc")
            (system / "KERNEL32.dll").write_bytes(b"system")
            imports = {"humtrace.exe": ["QT6CORE.DLL"],
                       "Qt6Core.dll": ["libgcc.dll", "KERNEL32.dll"],
                       "libgcc.dll": []}
            manifest = packaging.collect_dependencies(stage, runtime, system,
                                                       lambda p: imports[p.name])
            self.assertEqual((stage / "Qt6Core.dll").read_bytes(), b"qt")
            self.assertEqual((stage / "libgcc.dll").read_bytes(), b"gcc")
            self.assertFalse((stage / "KERNEL32.dll").exists())
            self.assertEqual(len(manifest), 3)

    def test_missing_dependency_fails_instead_of_publishing_incomplete_zip(self):
        with tempfile.TemporaryDirectory() as root:
            stage = Path(root) / "stage"
            stage.mkdir()
            (stage / "humtrace-gui.exe").write_bytes(b"app")
            with self.assertRaisesRegex(RuntimeError, "missing-runtime.dll"):
                packaging.collect_dependencies(stage, Path(root), Path(root),
                                                lambda _: ["missing-runtime.dll"])


if __name__ == "__main__":
    unittest.main()
