"""Stage upstream QtBase license texts with source provenance.

This is a narrow source-attribution aid. QtBase sources are an upstream
baseline, not a claim that they correspond exactly to patched Homebrew/MSYS2
builds or that all runtime dependencies have been inventoried.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile
import tempfile
import urllib.request


MAX_ARCHIVE_BYTES = 512 * 1024 * 1024
MAX_LICENSE_BYTES = 32 * 1024 * 1024
MAX_LICENSE_FILES = 1000
VERSION_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
SHA_RE = re.compile(r"^([0-9a-fA-F]{64})(?:\s+\*?([^\s]+))?\s*$")


def _hash(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _version_parts(version):
    match = VERSION_RE.fullmatch(version)
    if not match:
        raise RuntimeError(f"invalid Qt version (expected x.y.z): {version!r}")
    return match.groups()


def qt_source_urls(version):
    """Return official archive and SHA256 sidecar URLs for an exact QtBase version."""
    major, minor, _ = _version_parts(version)
    filename = f"qtbase-everywhere-src-{version}.tar.xz"
    base = f"https://download.qt.io/archive/qt/{major}.{minor}/{version}/submodules/{filename}"
    return base, base + ".sha256", filename


def parse_sha256(text, expected_filename):
    """Parse Qt's sha256 sidecar and require it to name the requested archive."""
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError("Qt checksum sidecar must contain exactly one non-empty line")
    match = SHA_RE.fullmatch(lines[0])
    if not match:
        raise RuntimeError("Qt checksum sidecar has an invalid SHA256 line")
    digest, named_file = match.groups()
    if named_file is not None and Path(named_file).name != expected_filename:
        raise RuntimeError(f"Qt checksum names {named_file!r}, expected {expected_filename!r}")
    if named_file is not None and ("/" in named_file or "\\" in named_file):
        raise RuntimeError("Qt checksum filename must be a basename")
    return digest.lower()


def _download(url, destination, max_bytes=MAX_ARCHIVE_BYTES):
    request = urllib.request.Request(url, headers={"User-Agent": "HumTrace source attribution"})
    size = 0
    with urllib.request.urlopen(request, timeout=60) as response, Path(destination).open("xb") as output:
        declared = response.headers.get("Content-Length")
        if declared and int(declared) > max_bytes:
            raise RuntimeError(f"download exceeds size limit ({declared} bytes): {url}")
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            size += len(chunk)
            if size > max_bytes:
                raise RuntimeError(f"download exceeds size limit ({max_bytes} bytes): {url}")
            output.write(chunk)
    if size == 0:
        raise RuntimeError(f"download was empty: {url}")


def _safe_license_members(archive, expected_root):
    members = []
    total_size = 0
    for member in archive.getmembers():
        raw = member.name
        path = PurePosixPath(raw)
        if path.is_absolute() or ".." in path.parts or "\\" in raw:
            # Reject unsafe names anywhere, rather than trusting a later
            # extraction filter to behave consistently across Python versions.
            raise RuntimeError(f"unsafe path in Qt source archive: {raw!r}")
        if not path.parts or path.parts[0] != expected_root:
            continue
        if len(path.parts) < 2 or path.parts[1] != "LICENSES":
            continue
        if member.isdir():
            continue
        if not member.isfile():
            raise RuntimeError(f"non-regular entry in Qt LICENSES tree: {raw!r}")
        if len(path.parts) < 3:
            raise RuntimeError(f"unexpected file at Qt LICENSES root: {raw!r}")
        total_size += member.size
        if total_size > MAX_LICENSE_BYTES:
            raise RuntimeError("Qt LICENSES tree exceeds extraction size limit")
        members.append((member, PurePosixPath(*path.parts[2:])))
        if len(members) > MAX_LICENSE_FILES:
            raise RuntimeError("Qt LICENSES tree exceeds file-count limit")
    if not members:
        raise RuntimeError(f"Qt source archive has no files beneath {expected_root}/LICENSES")
    return members


def extract_license_files(archive_path, version, destination):
    """Safely extract only regular files below the archive-root LICENSES dir."""
    _version_parts(version)
    expected_root = f"qtbase-everywhere-src-{version}"
    destination = Path(destination)
    total = 0
    files = []
    with tarfile.open(archive_path, mode="r:xz") as archive:
        members = _safe_license_members(archive, expected_root)
        for member, relative in members:
            target = destination.joinpath(*relative.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                raise RuntimeError(f"duplicate Qt license path in archive: {relative}")
            source = archive.extractfile(member)
            if source is None:
                raise RuntimeError(f"cannot read Qt license file: {member.name}")
            written = 0
            digest = hashlib.sha256()
            with source, target.open("xb") as output:
                while True:
                    chunk = source.read(1024 * 1024)
                    if not chunk:
                        break
                    written += len(chunk)
                    total += len(chunk)
                    if total > MAX_LICENSE_BYTES or written > member.size:
                        raise RuntimeError("Qt LICENSES extraction exceeded declared bounds")
                    digest.update(chunk)
                    output.write(chunk)
            if written != member.size:
                raise RuntimeError(f"Qt license file size mismatch: {member.name}")
            files.append({"path": relative.as_posix(), "size": written,
                          "sha256": digest.hexdigest()})
    return sorted(files, key=lambda item: item["path"])


def stage_qt_source(version, stage, archive=None, checksum=None, download=_download):
    """Download/verify official QtBase source and stage its LICENSES subtree."""
    _, _, filename = qt_source_urls(version)
    if (archive is None) != (checksum is None):
        raise RuntimeError("--archive and --checksum must be supplied together")
    stage = Path(stage)
    if not stage.is_dir():
        raise RuntimeError(f"stage directory does not exist: {stage}")
    sources_dir = stage / "sources"
    licenses_dir = stage / "licenses"
    if sources_dir.exists() or licenses_dir.exists() or (stage / "QTBASE_SOURCE.json").exists():
        raise RuntimeError("Qt source output already exists in stage; use a clean stage")

    archive_url, checksum_url, _ = qt_source_urls(version)
    with tempfile.TemporaryDirectory(prefix=".qtbase-source-", dir=stage) as work:
        work = Path(work)
        staged_sources = work / "sources"
        staged_sources.mkdir()
        archive_path = staged_sources / filename
        checksum_path = staged_sources / (filename + ".sha256")
        if archive is None:
            download(archive_url, archive_path)
            download(checksum_url, checksum_path, max_bytes=1024 * 1024)
        else:
            shutil.copyfile(archive, archive_path)
            shutil.copyfile(checksum, checksum_path)

        expected_hash = parse_sha256(checksum_path.read_text(encoding="ascii"), filename)
        actual_hash = _hash(archive_path)
        if actual_hash != expected_hash:
            raise RuntimeError(f"Qt source SHA256 mismatch: expected {expected_hash}, got {actual_hash}")

        staged_licenses = work / "licenses" / f"qtbase-{version}"
        files = extract_license_files(archive_path, version, staged_licenses)
        manifest = {
            "schema_version": 1,
            "component": "QtBase",
            "version": version,
            "source_url": archive_url,
            "checksum_url": checksum_url,
            "archive": f"sources/{filename}",
            "checksum_file": f"sources/{filename}.sha256",
            "sha256": actual_hash,
            "license_directory": f"licenses/qtbase-{version}",
            "license_files": files,
            "scope_note": (
                "Upstream QtBase source/license baseline only. Distribution-patched builds, "
                "other Qt modules, non-Qt dependencies, applicable license terms, and any "
                "corresponding-source or relinkability requirements require separate inventory and review."
            ),
        }
        (work / "QTBASE_SOURCE.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        staged_sources.rename(sources_dir)
        staged_licenses_dir = work / "licenses"
        staged_licenses_dir.rename(licenses_dir)
        (work / "QTBASE_SOURCE.json").rename(stage / "QTBASE_SOURCE.json")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="exact installed Qt version, for example 6.8.2")
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--archive", type=Path, help="pre-downloaded official .tar.xz (tests/offline use)")
    parser.add_argument("--checksum", type=Path, help="matching official .sha256 sidecar")
    args = parser.parse_args()
    stage_qt_source(args.version, args.stage, archive=args.archive, checksum=args.checksum)


if __name__ == "__main__":
    main()
