"""Bounded provenance collector tests using controlled MSYS2 page fixtures."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "package_msys2_sources", Path(__file__).resolve().parents[1] / "tools/package_msys2_sources.py")
sources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sources)


def package_page(owner, version, source_package=None, source_version=None):
    source_package = source_package or owner
    source_version = source_version or version
    filename = f"{source_package}-{source_version}.src.tar.zst"
    return ("<html><body><div>Version:</div><div>" + version + "</div>"
            "<a href=\"https://mirror.msys2.org/mingw/sources/" + filename + "\">"
            "Source-Only Tarball</a></body></html>").encode()


def official_style_package_page(owner, version, source_package=None, source_version=None):
    source_package = source_package or owner
    source_version = source_version or version
    filename = f"{source_package}-{source_version}.src.tar.zst"
    return ("<html><body><dl><dt>Version:</dt><dd>" + version + "</dd>"
            "<dt>Source-Only Tarball:</dt><dd><a href=\"https://mirror.msys2.org/mingw/sources/"
            + filename + "\">https://mirror.msys2.org/mingw/sources/" + filename
            + "</a></dd></dl></body></html>").encode()


class Msys2SourceRecipes(unittest.TestCase):
    def _run_fixture(self, root, owners, pages, archives, **kwargs):
        stage = root / "stage"
        stage.mkdir()
        manifest = root / "DEPENDENCY_LICENSES.json"
        manifest.write_text(json.dumps({"dependencies": owners}), encoding="utf-8")
        calls = []

        def fetch(url, limit):
            calls.append((url, limit))
            if url in pages:
                data = pages[url]
                return data, url
            for prefix, result in archives.items():
                if url == prefix:
                    return result
            raise AssertionError(f"unexpected URL: {url}")

        result = sources.collect_msys2_sources(manifest, stage, fetch=fetch, **kwargs)
        return result, stage, calls

    def test_downloads_exact_owner_versions_and_deduplicates_shared_source_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            owners = [
                {"package": "mingw-w64-ucrt-x86_64-libgcc", "version": "16.2.0-6"},
                {"package": "mingw-w64-ucrt-x86_64-libstdc++", "version": "16.2.0-6"},
            ]
            pages = {
                "https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libgcc": package_page(
                    "mingw-w64-ucrt-x86_64-libgcc", "16.2.0-6", "mingw-w64-gcc"),
                "https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libstdc++": package_page(
                    "mingw-w64-ucrt-x86_64-libstdc++", "16.2.0-6", "mingw-w64-gcc"),
            }
            archive_url = "https://mirror.msys2.org/mingw/sources/mingw-w64-gcc-16.2.0-6.src.tar.zst"
            payload = b"PKGBUILD and patch recipe fixture"
            result, stage, calls = self._run_fixture(root, owners, pages, {archive_url: (payload, archive_url)})
            self.assertEqual(sum(url == archive_url for url, _ in calls), 1)
            entries = result["packages"]
            self.assertEqual(len(entries), 2)
            self.assertEqual({entry["source_archive"] for entry in entries},
                             {"sources/mingw-w64-gcc-16.2.0-6.src.tar.zst"})
            self.assertEqual({entry["source_package"] for entry in entries}, {"mingw-w64-gcc"})
            self.assertTrue(all(entry["installed_version"] == "16.2.0-6" for entry in entries))
            record = result["archives"][0]
            self.assertEqual(record["sha256"], hashlib.sha256(payload).hexdigest())
            self.assertEqual(record["size"], len(payload))
            self.assertIn("locally computed", result["digest_scope"])
            self.assertEqual((stage / "MSYS2_SOURCE_RECIPES" / record["path"]).read_bytes(), payload)

    def test_parses_official_layout_with_source_label_outside_url_anchor(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            owner = "mingw-w64-ucrt-x86_64-qt6-base"
            page_url = f"https://packages.msys2.org/packages/{owner}"
            archive_url = "https://mirror.msys2.org/mingw/sources/mingw-w64-qt6-base-6.12.0-1.src.tar.zst"
            payload = b"official source recipe bytes"
            result, stage, calls = self._run_fixture(root, [{"package": owner, "version": "6.12.0-1"}],
                {page_url: official_style_package_page(owner, "6.12.0-1", "mingw-w64-qt6-base")},
                {archive_url: (payload, archive_url)})
            self.assertEqual(result["packages"][0]["source_url"], archive_url)
            self.assertEqual((stage / "MSYS2_SOURCE_RECIPES" / result["archives"][0]["path"]).read_bytes(), payload)

    def test_rejects_page_version_mismatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            owner = "pkg"
            with self.assertRaisesRegex(RuntimeError, "does not match installed version"):
                self._run_fixture(root, [{"package": owner, "version": "1.0-1"}], {
                    "https://packages.msys2.org/packages/pkg": package_page(owner, "1.0-2")}, {})
            self.assertFalse((root / "stage" / "MSYS2_SOURCE_RECIPES").exists())

    def test_rejects_unapproved_archive_host_and_path_escape(self):
        for href in ("https://evil.example/mingw/sources/pkg-1.0-1.src.tar.zst",
                     "https://mirror.msys2.org/other/pkg-1.0-1.src.tar.zst",
                     "https://mirror.msys2.org/mingw/sources/../pkg-1.0-1.src.tar.zst"):
            with self.subTest(href=href), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                html = (f"<div>Version:</div><div>1.0-1</div><a href=\"{href}\">"
                        "Source-Only Tarball</a>").encode()
                with self.assertRaisesRegex(RuntimeError, "source archive URL"):
                    self._run_fixture(root, [{"package": "pkg", "version": "1.0-1"}],
                        {"https://packages.msys2.org/packages/pkg": html}, {})

    def test_rejects_source_archive_basename_with_different_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            owner = "pkg"
            html = ("<div>Version:</div><div>1.0-1</div><a href=\""
                    "https://mirror.msys2.org/mingw/sources/pkg-1.0-2.src.tar.zst\">"
                    "Source-Only Tarball</a>").encode()
            with self.assertRaisesRegex(RuntimeError, "basename/version mismatch"):
                self._run_fixture(root, [{"package": owner, "version": "1.0-1"}],
                                  {"https://packages.msys2.org/packages/pkg": html}, {})

    def test_rejects_oversized_package_page_and_owner_count(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            html = package_page("pkg", "1.0-1") + b" " * 50
            with self.assertRaisesRegex(RuntimeError, "package page exceeds size limit"):
                self._run_fixture(root, [{"package": "pkg", "version": "1.0-1"}],
                    {"https://packages.msys2.org/packages/pkg": html}, {}, max_html_bytes=16)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(RuntimeError, "owner count exceeds limit"):
                self._run_fixture(root, [{"package": "pkg", "version": "1.0-1"}], {}, {},
                                  max_packages=0)

    def test_rejects_oversized_dependency_manifest_before_json_decode(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "DEPENDENCY_LICENSES.json"
            manifest.write_bytes(b" " * (sources.MAX_DEPENDENCY_MANIFEST_BYTES + 1))
            stage = root / "stage"
            stage.mkdir()
            with self.assertRaisesRegex(RuntimeError, "dependency license manifest exceeds size limit"):
                sources.collect_msys2_sources(manifest, stage, fetch=lambda url, limit: self.fail("network must not run"))

    def test_rejects_allowed_host_redirect_to_different_archive_filename(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            owner = "pkg"
            page = "https://packages.msys2.org/packages/pkg"
            archive = "https://mirror.msys2.org/mingw/sources/pkg-1.0-1.src.tar.zst"
            stage = root / "stage"
            stage.mkdir()
            manifest = root / "DEPENDENCY_LICENSES.json"
            manifest.write_text(json.dumps({"dependencies": [{"package": owner, "version": "1.0-1"}]}), encoding="utf-8")

            def fetch(url, limit):
                if url == page:
                    return package_page(owner, "1.0-1"), page
                return b"tarball", "https://repo.msys2.org/mingw/sources/other-1.0-1.src.tar.zst"

            with self.assertRaisesRegex(RuntimeError, "redirected to a different filename"):
                sources.collect_msys2_sources(manifest, stage, fetch=fetch)
            self.assertFalse((stage / "MSYS2_SOURCE_RECIPES").exists())

    def test_rejects_redirect_outside_allowlist(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            owner = "pkg"
            page = "https://packages.msys2.org/packages/pkg"
            archive = "https://mirror.msys2.org/mingw/sources/pkg-1.0-1.src.tar.zst"
            stage = root / "stage"
            stage.mkdir()
            manifest = root / "DEPENDENCY_LICENSES.json"
            manifest.write_text(json.dumps({"dependencies": [{"package": owner, "version": "1.0-1"}]}), encoding="utf-8")

            def fetch(url, limit):
                if url == page:
                    return package_page(owner, "1.0-1"), page
                return b"tarball", "https://unapproved.example/pkg.src.tar.zst"

            with self.assertRaisesRegex(RuntimeError, "redirected to an unapproved"):
                sources.collect_msys2_sources(manifest, stage, fetch=fetch)
            self.assertFalse((stage / "MSYS2_SOURCE_RECIPES").exists())
            self.assertEqual(list(stage.glob(".msys2-source-*")), [])

    def test_enforces_archive_and_total_size_limits_and_cleans_up(self):
        cases = [
            ({"max_archive_bytes": 4}, b"12345", "exceeds size limit"),
            ({"max_total_bytes": 4}, b"12345", "total size limit"),
        ]
        for options, payload, message in cases:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                owner = "pkg"
                page = "https://packages.msys2.org/packages/pkg"
                archive = "https://mirror.msys2.org/mingw/sources/pkg-1.0-1.src.tar.zst"
                manifest = root / "DEPENDENCY_LICENSES.json"
                manifest.write_text(json.dumps({"dependencies": [{"package": owner, "version": "1.0-1"}]}), encoding="utf-8")
                stage = root / "stage"
                stage.mkdir()

                def fetch(url, limit):
                    if url == page:
                        return package_page(owner, "1.0-1"), page
                    return payload, archive

                with self.assertRaisesRegex(RuntimeError, message):
                    sources.collect_msys2_sources(manifest, stage, fetch=fetch, **options)
                self.assertFalse((stage / "MSYS2_SOURCE_RECIPES").exists())
                self.assertEqual(list(stage.glob(".msys2-source-*")), [])

    def test_cleans_up_after_later_archive_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = root / "stage"
            stage.mkdir()
            owners = [{"package": name, "version": "1.0-1"} for name in ("pkg-a", "pkg-b")]
            manifest = root / "DEPENDENCY_LICENSES.json"
            manifest.write_text(json.dumps({"dependencies": owners}), encoding="utf-8")
            first_page = "https://packages.msys2.org/packages/pkg-a"
            second_page = "https://packages.msys2.org/packages/pkg-b"
            first_archive = "https://mirror.msys2.org/mingw/sources/pkg-a-1.0-1.src.tar.zst"
            second_archive = "https://mirror.msys2.org/mingw/sources/pkg-b-1.0-1.src.tar.zst"

            def fetch(url, limit):
                if url == first_page:
                    return package_page("pkg-a", "1.0-1"), first_page
                if url == second_page:
                    return package_page("pkg-b", "1.0-1"), second_page
                if url == first_archive:
                    return b"one", first_archive
                raise RuntimeError("simulated download failure")

            with self.assertRaisesRegex(RuntimeError, "simulated download failure"):
                sources.collect_msys2_sources(manifest, stage, fetch=fetch)
            self.assertEqual(list(stage.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
