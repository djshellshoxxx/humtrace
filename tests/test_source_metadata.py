"""Configure-only source metadata regressions; run with --cmake /path/to/cmake.

This test is optional for the Make CLI workflow and requires no Qt or app build.
The archive fixture uses the real CMakeLists.txt and minimal configure inputs.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]
CMAKE = shutil.which("cmake")


class SourceMetadata(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if CMAKE is None:
            raise unittest.SkipTest("CMake unavailable; run this test with --cmake /path/to/cmake")

    def configure(self, source, build, *options):
        command = [CMAKE, "-S", str(source), "-B", str(build),
                   "-DHUMTRACE_BUILD_GUI=OFF", "-DHUMTRACE_BUILD_TESTS=OFF", *options]
        if shutil.which("ninja"):
            command.extend(["-G", "Ninja"])
        environment = dict(os.environ)
        for key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE"):
            environment.pop(key, None)
        return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", env=environment,
                              check=False, timeout=120)

    def archive(self, root):
        source = root / "source archive"
        source.mkdir()
        shutil.copy2(SOURCE / "CMakeLists.txt", source / "CMakeLists.txt")
        for relative, text in {
            "src/core.cpp": "void metadata_fixture() {}\n",
            "src/main.cpp": "int main() { return 0; }\n",
            "app/presentation/findings.cpp": "void presentation_fixture() {}\n",
        }.items():
            path = source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        return source

    def test_supplied_full_revision_is_recorded_without_git_discovery(self):
        for revision in ("0123456789abcdef" * 2 + "01234567", "a1" * 32, "ABCDEF01" * 8, "0" * 40):
            with self.subTest(bits=len(revision)), tempfile.TemporaryDirectory(prefix="humtrace-source-") as temporary:
                root = Path(temporary)
                build = root / "build"
                result = self.configure(self.archive(root), build,
                    "-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE", "-DHUMTRACE_SOURCE_REVISION=" + revision)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((build / "SOURCE_COMMIT.txt").read_text(encoding="utf-8").strip(), revision)

    def test_invalid_explicit_revision_is_rejected_instead_of_replaced_by_fallback(self):
        for revision in ("g" * 40, "a" * 39, "a" * 41, "a" * 63, "a" * 65, "not-a-revision"):
            with self.subTest(revision=revision), tempfile.TemporaryDirectory(prefix="humtrace-source-") as temporary:
                root = Path(temporary)
                build = root / "build"
                result = self.configure(self.archive(root), build, "-DHUMTRACE_SOURCE_REVISION=" + revision)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("HUMTRACE_SOURCE_REVISION", result.stdout + result.stderr)
                self.assertFalse((build / "SOURCE_COMMIT.txt").exists())

    def test_archive_without_git_or_override_retains_explicit_unknown_marker(self):
        with tempfile.TemporaryDirectory(prefix="humtrace-source-") as temporary:
            root = Path(temporary)
            build = root / "build"
            result = self.configure(self.archive(root), build, "-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((build / "SOURCE_COMMIT.txt").read_text(encoding="utf-8").strip(),
                             "unknown (source archive without Git metadata)")

    def test_ordinary_git_checkout_records_its_actual_head(self):
        with tempfile.TemporaryDirectory(prefix="humtrace-git-source-") as temporary:
            build = Path(temporary) / "build"
            result = self.configure(SOURCE, build)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            git = None
            for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
                if line.startswith("GIT_EXECUTABLE:FILEPATH="):
                    git = line.partition("=")[2]
                    break
            recorded = (build / "SOURCE_COMMIT.txt").read_text(encoding="utf-8").strip()
            if git and Path(git).is_file():
                head = subprocess.run([git, "rev-parse", "HEAD"], cwd=SOURCE, text=True, encoding="utf-8",
                                      capture_output=True, check=False, timeout=30)
                self.assertEqual(head.returncode, 0, head.stderr)
                self.assertEqual(recorded, head.stdout.strip())
            else:
                self.assertEqual(recorded, "unknown (source archive without Git metadata)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default=CMAKE)
    options, unittest_options = parser.parse_known_args()
    if options.cmake is None:
        parser.error("CMake unavailable; supply --cmake /path/to/cmake")
    CMAKE = shutil.which(options.cmake) or str(Path(options.cmake).resolve())
    if not Path(CMAKE).is_file():
        parser.error("--cmake does not name an executable file")
    unittest.main(argv=[sys.argv[0], *unittest_options])
