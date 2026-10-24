"""Package source-commit provenance checks."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "smoke_package", Path(__file__).resolve().parents[1] / "tools/smoke_package.py")
smoke_package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke_package)


class PackageSourceCommit(unittest.TestCase):
    def test_accepts_matching_sha_with_crlf(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "SOURCE_COMMIT.txt"
            sha = "a" * 40
            source.write_bytes((sha + "\r\n").encode("ascii"))
            self.assertEqual(smoke_package.validate_source_commit(sha, source), sha)

    def test_rejects_mismatched_or_unknown_commit(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "SOURCE_COMMIT.txt"
            source.write_text("unknown\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "does not match expected source commit"):
                smoke_package.validate_source_commit("b" * 40, source)
            with self.assertRaisesRegex(RuntimeError, "40 or 64 hexadecimal"):
                smoke_package.validate_source_commit("unknown", source)

    def test_rejects_missing_source_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(RuntimeError, "cannot read source commit file"):
                smoke_package.validate_source_commit("c" * 40, Path(temporary) / "missing.txt")

    def test_rejects_oversized_source_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "SOURCE_COMMIT.txt"
            source.write_bytes(b"a" * (smoke_package.MAX_SOURCE_FILE_BYTES + 1))
            with self.assertRaisesRegex(RuntimeError, "source commit file is too large"):
                smoke_package.validate_source_commit("a" * 40, source)

    def test_rejects_invalid_expected_sha(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "SOURCE_COMMIT.txt"
            source.write_text("a" * 40, encoding="utf-8")
            for invalid in ("unknown", "a" * 39, "g" * 40, "a" * 41):
                with self.subTest(invalid=invalid), self.assertRaisesRegex(RuntimeError, "40 or 64 hexadecimal"):
                    smoke_package.validate_source_commit(invalid, source)

    def test_smoke_entrypoint_checks_metadata_before_launching_binaries(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "SOURCE_COMMIT.txt"
            source.write_text("wrong\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "does not match expected source commit"):
                smoke_package.main(["--cli", "missing-cli", "--gui", "missing-gui",
                                    "--expected-source-sha", "d" * 40,
                                    "--source-file", str(source)])


if __name__ == "__main__":
    unittest.main()
