import hashlib
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_qt_sources", ROOT / "tools" / "package_qt_sources.py")
qt_sources = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(qt_sources)


def make_archive(path, version="6.8.2", entries=None):
    root = f"qtbase-everywhere-src-{version}"
    entries = entries if entries is not None else {
        f"{root}/LICENSES/LicenseRef-Qt-Commercial.txt": b"Qt license text\n",
        f"{root}/LICENSES/GPL-3.0-only.txt": b"GPL text\n",
        f"{root}/README": b"not a license file",
    }
    with tarfile.open(path, "w:xz") as archive:
        for name, data in entries.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))


class QtSourceTests(unittest.TestCase):
    def test_official_url_and_checksum_sidecar_representation(self):
        archive, checksum, filename = qt_sources.qt_source_urls("6.8.2")
        self.assertEqual(filename, "qtbase-everywhere-src-6.8.2.tar.xz")
        self.assertEqual(archive, "https://download.qt.io/archive/qt/6.8/6.8.2/submodules/" + filename)
        self.assertEqual(checksum, archive + ".sha256")
        digest = qt_sources.parse_sha256("012043ce6d411e6e8a91fdc4e05e6bedcfa10fcb1347d3c33908f7fdd10dfe05  " + filename + "\n", filename)
        self.assertEqual(digest, "012043ce6d411e6e8a91fdc4e05e6bedcfa10fcb1347d3c33908f7fdd10dfe05")
        with self.assertRaisesRegex(RuntimeError, "names"):
            qt_sources.parse_sha256("0" * 64 + " other.tar.xz\n", filename)

    def test_extract_only_license_tree_and_hash_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive_path = root / "qt.tar.xz"
            make_archive(archive_path)
            destination = root / "out"
            records = qt_sources.extract_license_files(archive_path, "6.8.2", destination)
            self.assertEqual([r["path"] for r in records], ["GPL-3.0-only.txt", "LicenseRef-Qt-Commercial.txt"])
            self.assertFalse((destination / "README").exists())
            self.assertEqual((destination / "GPL-3.0-only.txt").read_bytes(), b"GPL text\n")
            self.assertEqual(records[0]["sha256"], hashlib.sha256(b"GPL text\n").hexdigest())

    def test_reject_unsafe_paths_and_version_mismatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive_path = root / "unsafe.tar.xz"
            make_archive(archive_path, entries={
                "qtbase-everywhere-src-6.8.2/LICENSES/../../escape": b"bad",
            })
            with self.assertRaisesRegex(RuntimeError, "unsafe path"):
                qt_sources.extract_license_files(archive_path, "6.8.2", root / "out")
            make_archive(archive_path, version="6.8.1")
            with self.assertRaisesRegex(RuntimeError, "no files"):
                qt_sources.extract_license_files(archive_path, "6.8.2", root / "out2")
            with self.assertRaisesRegex(RuntimeError, "invalid Qt version"):
                qt_sources.qt_source_urls("6.8/latest")

    def test_stage_verifies_checksum_preserves_archive_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = root / "stage"
            stage.mkdir()
            archive = root / "qtbase-everywhere-src-6.8.2.tar.xz"
            make_archive(archive)
            digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            checksum = root / "qt.sha256"
            checksum.write_text(digest + "  " + archive.name + "\n", encoding="ascii")
            manifest = qt_sources.stage_qt_source("6.8.2", stage, archive, checksum)
            self.assertEqual(manifest["sha256"], digest)
            self.assertTrue((stage / "sources" / archive.name).is_file())
            self.assertTrue((stage / "sources" / (archive.name + ".sha256")).is_file())
            self.assertIn("separate inventory and review", manifest["scope_note"])
            with self.assertRaisesRegex(RuntimeError, "already exists"):
                qt_sources.stage_qt_source("6.8.2", stage, archive, checksum)

    def test_stage_rejects_bad_checksum_before_extraction(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = root / "stage"
            stage.mkdir()
            archive = root / "qtbase-everywhere-src-6.8.2.tar.xz"
            make_archive(archive)
            checksum = root / "qt.sha256"
            checksum.write_text("0" * 64 + "  " + archive.name + "\n", encoding="ascii")
            with self.assertRaisesRegex(RuntimeError, "SHA256 mismatch"):
                qt_sources.stage_qt_source("6.8.2", stage, archive, checksum)
            self.assertFalse((stage / "licenses").exists())
            self.assertFalse((stage / "sources").exists())
            self.assertEqual(list(stage.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
