"""Stage exact-version MSYS2 source-only recipe archives for bundled DLL owners.

The source-only archives contain MSYS2 build recipes and patch payloads, not
the upstream source archives referenced by those recipes. Hashes here are
computed locally over downloads from official HTTPS endpoints; they are not
upstream-published checksums or signatures and establish no legal conclusion.
"""
import argparse
from html.parser import HTMLParser
import hashlib
import json
import os
from pathlib import Path
from pathlib import PurePosixPath
import re
import shutil
import tempfile
from urllib.error import HTTPError, URLError
from urllib.parse import quote, unquote, urljoin, urlparse
from urllib.request import HTTPRedirectHandler, Request, build_opener

MAX_PACKAGES = 64
MAX_HTML_BYTES = 2 * 1024 * 1024
MAX_ARCHIVE_BYTES = 256 * 1024 * 1024
MAX_TOTAL_BYTES = 512 * 1024 * 1024
MAX_DEPENDENCY_MANIFEST_BYTES = 2 * 1024 * 1024
REQUEST_TIMEOUT_SECONDS = 20
OUTPUT_DIRECTORY = "MSYS2_SOURCE_RECIPES"
# Primary and Tier 1 hosts listed by https://www.msys2.org/dev/mirrors/.
MSYS2_MIRROR_HOSTS = frozenset({
    "mirror.msys2.org", "repo.msys2.org",
    "mirror.umd.edu", "mirror.accum.se", "ftp.nluug.nl", "ftp2.osuosl.org",
    "mirror.internet.asn.au", "mirror.selfnet.de", "mirror.yandex.ru",
    "mirrors.dotsrc.org", "mirrors.tuna.tsinghua.edu.cn", "mirrors.ustc.edu.cn",
    "mirror.nju.edu.cn", "mirrors.bfsu.edu.cn", "mirror.clarkson.edu",
    "distrohub.kyiv.ua", "mirror.archlinux.tw", "us.mirrors.cicku.me",
    "ca.mirrors.cicku.me", "mirrors.qlu.edu.cn",
})


def _safe_url(url, kind):
    parsed = urlparse(url)
    if parsed.scheme != "https" or parsed.username or parsed.password or parsed.fragment or parsed.query:
        raise RuntimeError(f"unapproved {kind} URL: {url}")
    host = (parsed.hostname or "").lower()
    if parsed.port not in (None, 443):
        raise RuntimeError(f"unapproved {kind} URL: {url}")
    if kind == "package page":
        accepted = host == "packages.msys2.org" and parsed.path.startswith("/packages/")
    else:
        path = unquote(parsed.path)
        path_parts = PurePosixPath(path).parts
        accepted = (host in MSYS2_MIRROR_HOSTS
                    and len(path_parts) >= 3
                    and path_parts[-3:-1] == ("mingw", "sources")
                    and PurePosixPath(path).name.endswith(".src.tar.zst"))
    if not accepted or ".." in PurePosixPath(unquote(parsed.path)).parts:
        raise RuntimeError(f"unapproved {kind} URL: {url}")
    return url


class _RedirectPolicy(HTTPRedirectHandler):
    def __init__(self, kind):
        super().__init__()
        self.kind = kind

    def redirect_request(self, request, response, code, message, headers, new_url):
        _safe_url(new_url, self.kind)
        return super().redirect_request(request, response, code, message, headers, new_url)


def _download(url, limit, kind):
    _safe_url(url, kind)
    opener = build_opener(_RedirectPolicy(kind))
    request = Request(url, headers={"User-Agent": "HumTrace MSYS2 source provenance", "Accept-Encoding": "identity"})
    try:
        with opener.open(request, timeout=REQUEST_TIMEOUT_SECONDS) as response:
            final_url = response.geturl()
            _safe_url(final_url, kind)
            declared = response.headers.get("Content-Length")
            if declared and int(declared) > limit:
                raise RuntimeError(f"response exceeds size limit ({declared} > {limit} bytes)")
            data = response.read(limit + 1)
    except (HTTPError, URLError, TimeoutError, OSError) as error:
        raise RuntimeError(f"download failed for {url}: {error}") from error
    if len(data) > limit:
        raise RuntimeError("response exceeds size limit")
    return data, final_url


class _PageParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.text = []
        self.links = []
        self._link_stack = []

    def handle_starttag(self, tag, attrs):
        if tag.casefold() == "a":
            href = dict(attrs).get("href")
            self._link_stack.append({"href": href, "text": []})

    def handle_endtag(self, tag):
        if tag.casefold() == "a" and self._link_stack:
            self.links.append(self._link_stack.pop())

    def handle_data(self, data):
        text = " ".join(data.split())
        if text:
            self.text.append(text)
            for link in self._link_stack:
                link["text"].append(text)


def _parse_package_page(html, owner, expected_version, page_url):
    if len(html) > MAX_HTML_BYTES:
        raise RuntimeError(f"MSYS2 package page exceeds size limit: {owner}")
    try:
        decoded = html.decode("utf-8")
    except UnicodeDecodeError as error:
        raise RuntimeError(f"MSYS2 package page is not UTF-8 for {owner}") from error
    parser = _PageParser()
    parser.feed(decoded)
    text = " ".join(parser.text)
    match = re.search(r"\bVersion\s*:\s*([A-Za-z0-9.+_~:-]+)", text, re.IGNORECASE)
    if match is None:
        raise RuntimeError(f"cannot find displayed package version for {owner}")
    displayed_version = match.group(1)
    if displayed_version != expected_version:
        raise RuntimeError(f"MSYS2 page version {displayed_version} does not match installed version "
                           f"{expected_version} for {owner}")

    candidates = []
    for link in parser.links:
        href = link.get("href")
        if href:
            candidate = urljoin(page_url, href)
            if PurePosixPath(unquote(urlparse(candidate).path)).name.endswith(".src.tar.zst"):
                candidates.append(candidate)
    if len(candidates) != 1:
        raise RuntimeError(f"expected one MSYS2 source-only tarball link for {owner}, found {len(candidates)}")
    source_url = _safe_url(candidates[0], "source archive")
    filename = PurePosixPath(unquote(urlparse(source_url).path)).name
    suffix = f"-{expected_version}.src.tar.zst"
    if not filename.endswith(suffix) or filename == suffix:
        raise RuntimeError(f"MSYS2 source archive basename/version mismatch for {owner}: {filename}")
    source_package = filename[:-len(suffix)]
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+~-]*", source_package):
        raise RuntimeError(f"invalid MSYS2 source package basename: {filename}")
    return {"displayed_version": displayed_version, "source_url": source_url,
            "source_filename": filename, "source_package": source_package}


def _read_dependency_owners(manifest_path):
    try:
        with Path(manifest_path).open("rb") as source:
            raw_manifest = source.read(MAX_DEPENDENCY_MANIFEST_BYTES + 1)
    except OSError as error:
        raise RuntimeError(f"cannot read dependency license manifest: {error}") from error
    if len(raw_manifest) > MAX_DEPENDENCY_MANIFEST_BYTES:
        raise RuntimeError("dependency license manifest exceeds size limit")
    try:
        manifest = json.loads(raw_manifest.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot parse dependency license manifest: {error}") from error
    dependencies = manifest.get("dependencies") if isinstance(manifest, dict) else None
    if not isinstance(dependencies, list):
        raise RuntimeError("DEPENDENCY_LICENSES.json must contain a dependencies list")
    owners = {}
    for item in dependencies:
        if not isinstance(item, dict):
            raise RuntimeError("invalid dependency record in DEPENDENCY_LICENSES.json")
        owner, version = item.get("package"), item.get("version")
        if (not isinstance(owner, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+~-]*", owner)
                or not isinstance(version, str) or not re.fullmatch(r"[A-Za-z0-9.+_~:-]+", version)):
            raise RuntimeError("invalid package owner/version in DEPENDENCY_LICENSES.json")
        if owner in owners and owners[owner] != version:
            raise RuntimeError(f"multiple installed versions recorded for package {owner}")
        owners[owner] = version
    if not owners:
        raise RuntimeError("DEPENDENCY_LICENSES.json contains no runtime package owners")
    if len(owners) > MAX_PACKAGES:
        raise RuntimeError(f"runtime package owner count exceeds limit ({MAX_PACKAGES})")
    return sorted(owners.items())


def _sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def collect_msys2_sources(dependency_manifest, stage, fetch=None, *,
                          max_packages=MAX_PACKAGES, max_html_bytes=MAX_HTML_BYTES,
                          max_archive_bytes=MAX_ARCHIVE_BYTES, max_total_bytes=MAX_TOTAL_BYTES):
    """Download exact-version source-only MSYS2 recipe archives atomically."""
    stage = Path(stage)
    if not stage.is_dir():
        raise RuntimeError("package stage must be an existing directory")
    output = stage / OUTPUT_DIRECTORY
    if output.exists():
        raise RuntimeError(f"source recipe output already exists: {output}")
    owners = _read_dependency_owners(dependency_manifest)
    if len(owners) > max_packages:
        raise RuntimeError(f"runtime package owner count exceeds limit ({max_packages})")

    if fetch is None:
        def fetch(url, limit):
            kind = "package page" if urlparse(url).hostname == "packages.msys2.org" else "source archive"
            return _download(url, limit, kind)

    temporary = Path(tempfile.mkdtemp(prefix=".msys2-source-", dir=stage))
    try:
        sources_dir = temporary / "sources"
        sources_dir.mkdir()
        packages = []
        archives = {}
        total_size = 0
        for owner, version in owners:
            page_url = f"https://packages.msys2.org/packages/{quote(owner, safe='+._~-')}"
            _safe_url(page_url, "package page")
            try:
                page_bytes, page_final_url = fetch(page_url, max_html_bytes)
            except RuntimeError as error:
                if "exceeds size limit" in str(error):
                    raise RuntimeError(f"MSYS2 package page exceeds size limit: {owner}") from error
                raise
            try:
                _safe_url(page_final_url, "package page")
            except RuntimeError as error:
                raise RuntimeError(f"download redirected to an unapproved package page: {page_final_url}") from error
            if urlparse(page_final_url).path.rstrip("/") != urlparse(page_url).path.rstrip("/"):
                raise RuntimeError(f"MSYS2 package page redirected to a different package: {page_final_url}")
            if len(page_bytes) > max_html_bytes:
                raise RuntimeError(f"MSYS2 package page exceeds size limit: {owner}")
            page = _parse_package_page(page_bytes, owner, version, page_url)
            source_url = page["source_url"]
            filename = page["source_filename"]
            if source_url not in archives:
                if filename in {record["filename"] for record in archives.values()}:
                    raise RuntimeError(f"different source archives share filename: {filename}")
                remaining = max_total_bytes - total_size
                if remaining <= 0:
                    raise RuntimeError("MSYS2 source archives exceed total size limit")
                limit = min(max_archive_bytes, remaining)
                try:
                    archive_bytes, archive_final_url = fetch(source_url, limit)
                except RuntimeError as error:
                    if "exceeds size limit" in str(error):
                        if remaining < max_archive_bytes:
                            raise RuntimeError("MSYS2 source archives exceed total size limit") from error
                        raise RuntimeError(f"MSYS2 source archive exceeds size limit: {filename}") from error
                    raise
                try:
                    _safe_url(archive_final_url, "source archive")
                except RuntimeError as error:
                    raise RuntimeError(f"download redirected to an unapproved source archive URL: {archive_final_url}") from error
                if PurePosixPath(unquote(urlparse(archive_final_url).path)).name != filename:
                    raise RuntimeError(f"MSYS2 source archive redirected to a different filename: {archive_final_url}")
                if len(archive_bytes) > max_archive_bytes:
                    raise RuntimeError(f"MSYS2 source archive exceeds size limit: {filename}")
                if len(archive_bytes) > remaining:
                    raise RuntimeError("MSYS2 source archives exceed total size limit")
                if not archive_bytes:
                    raise RuntimeError(f"MSYS2 source archive is empty: {filename}")
                relative = Path("sources") / filename
                (temporary / relative).write_bytes(archive_bytes)
                record = {"filename": filename, "path": relative.as_posix(),
                          "source_url": source_url, "final_url": archive_final_url,
                          "size": len(archive_bytes), "sha256": _sha256_bytes(archive_bytes)}
                archives[source_url] = record
                total_size += len(archive_bytes)
            record = archives[source_url]
            if record["filename"] != filename:
                raise RuntimeError(f"shared source URL has inconsistent archive filename: {source_url}")
            packages.append({"installed_owner": owner, "installed_version": version,
                             "package_page_url": page_url, "displayed_version": page["displayed_version"],
                             "source_package": page["source_package"],
                             "source_url": source_url, "source_archive": record["path"]})

        result = {
            "schema_version": 1,
            "scope": "MSYS2 source-only package archives contain build recipes and patch payloads, not the upstream source archives referenced by those recipes.",
            "digest_scope": "SHA-256 values are locally computed over downloads from official HTTPS endpoints; no upstream-authenticated source-archive checksum or signature is claimed.",
            "total_archive_bytes": total_size,
            "packages": packages,
            "archives": sorted(archives.values(), key=lambda record: record["filename"]),
        }
        (temporary / "MANIFEST.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        os.replace(temporary, output)
        return result
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path,
                        help="DEPENDENCY_LICENSES.json with exact package owners and versions")
    parser.add_argument("--stage", required=True, type=Path,
                        help="existing package stage")
    args = parser.parse_args()
    result = collect_msys2_sources(args.manifest, args.stage)
    print(f"Staged {len(result['archives'])} MSYS2 source-only recipe archives for "
          f"{len(result['packages'])} runtime package owners.")


if __name__ == "__main__":
    main()
