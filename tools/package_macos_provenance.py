"""Record cautious Homebrew formula candidates for bundled macOS Mach-O images.

Names are matched exactly against files in installed Cellar kegs. This is
provenance evidence, not proof of binary correspondence, complete source, or
license compliance.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import tempfile

MAX_JSON_BYTES = 16 * 1024 * 1024
MAX_FORMULAE = 512
MAX_IMAGES = 256
MAX_CELLAR_ENTRIES = 250_000
MAX_CANDIDATES_PER_IMAGE = 32
MAX_HASH_BYTES = 512 * 1024 * 1024
MAX_TOTAL_HASH_BYTES = 2 * 1024 * 1024 * 1024
_FORMULA = re.compile(r"[A-Za-z0-9@+._-]{1,128}\Z")
_VERSION = re.compile(r"[A-Za-z0-9@+._~-]{1,128}\Z")
_SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _read_json(path, label):
    path = Path(path)
    try:
        size = path.stat().st_size
        if size > MAX_JSON_BYTES:
            raise RuntimeError(f"{label} exceeds size limit")
        raw = path.read_bytes()
        value = json.loads(raw.decode("utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot read {label}: {error}") from error
    return value, raw


def _image_records(value):
    if not isinstance(value, dict) or value.get("schema_version") != 1:
        raise RuntimeError("unsupported macOS dependency manifest")
    images = value.get("images")
    if not isinstance(images, list) or not 1 <= len(images) <= MAX_IMAGES:
        raise RuntimeError("invalid macOS dependency image list")
    seen = set()
    records = []
    for image in images:
        if not isinstance(image, dict):
            raise RuntimeError("invalid macOS dependency image record")
        filename = image.get("file")
        image_hash = image.get("sha256")
        if (not isinstance(filename, str) or len(filename) > 1024 or "\\" in filename
                or any(ord(char) < 32 for char in filename)
                or PurePosixPath(filename).is_absolute()
                or any(part in ("", ".", "..") for part in PurePosixPath(filename).parts)
                or not _SHA256.fullmatch(image_hash or "")):
            raise RuntimeError(f"unsafe or incomplete dependency image record: {filename!r}")
        if filename in seen:
            raise RuntimeError(f"duplicate dependency image path: {filename}")
        seen.add(filename)
        records.append({"file": filename, "sha256": image_hash, "basename": PurePosixPath(filename).name})
    return records


def _formula_records(value, cellar):
    if not isinstance(value, list) or len(value) > MAX_FORMULAE:
        raise RuntimeError("invalid or excessive Homebrew formula inventory")
    records = []
    seen = set()
    for formula in value:
        if not isinstance(formula, dict):
            raise RuntimeError("invalid Homebrew formula record")
        name = formula.get("name")
        full_name = formula.get("full_name", name)
        tap = formula.get("tap")
        if not isinstance(name, str) or name in (".", "..") or not _FORMULA.fullmatch(name):
            raise RuntimeError(f"invalid Homebrew formula name: {name!r}")
        if not isinstance(full_name, str) or len(full_name) > 256 or not full_name:
            raise RuntimeError(f"invalid Homebrew formula full name: {full_name!r}")
        if tap is not None and (not isinstance(tap, str) or len(tap) > 256):
            raise RuntimeError(f"invalid Homebrew formula tap: {tap!r}")
        installed = formula.get("installed")
        if not isinstance(installed, list) or not installed:
            raise RuntimeError(f"Homebrew formula is not installed: {name}")
        urls = formula.get("urls", {})
        stable = urls.get("stable", {}) if isinstance(urls, dict) else {}
        if not isinstance(stable, dict):
            stable = {}
        stable_url = stable.get("url")
        stable_checksum = stable.get("checksum")
        if stable_url is not None and (not isinstance(stable_url, str) or len(stable_url) > 4096):
            raise RuntimeError(f"invalid source URL for Homebrew formula: {name}")
        if stable_checksum is not None and (not isinstance(stable_checksum, str) or len(stable_checksum) > 256):
            raise RuntimeError(f"invalid source checksum for Homebrew formula: {name}")
        dependencies = formula.get("dependencies", [])
        if (not isinstance(dependencies, list) or len(dependencies) > MAX_FORMULAE
                or any(not isinstance(item, str) or len(item) > 256 for item in dependencies)):
            raise RuntimeError(f"invalid dependencies for Homebrew formula: {name}")
        license_value = formula.get("license")
        if (license_value is not None
                and not (isinstance(license_value, str) and len(license_value) <= 4096)
                and not (isinstance(license_value, list) and len(license_value) <= 64
                         and all(isinstance(item, str) and len(item) <= 1024 for item in license_value))):
            raise RuntimeError(f"invalid license metadata for Homebrew formula: {name}")
        homepage = formula.get("homepage")
        if homepage is not None and (not isinstance(homepage, str) or len(homepage) > 4096):
            raise RuntimeError(f"invalid homepage for Homebrew formula: {name}")
        for item in installed:
            if not isinstance(item, dict):
                raise RuntimeError(f"invalid installed version for Homebrew formula: {name}")
            version = item.get("version")
            if not isinstance(version, str) or version in (".", "..") or not _VERSION.fullmatch(version):
                raise RuntimeError(f"invalid installed version for Homebrew formula: {name}")
            key = (name, version)
            if key in seen:
                raise RuntimeError(f"duplicate installed Homebrew formula version: {name} {version}")
            seen.add(key)
            keg = cellar / name / version
            try:
                resolved_keg = keg.resolve(strict=True)
            except OSError as error:
                raise RuntimeError(f"installed Homebrew keg is absent: {name} {version}") from error
            cellar_root = cellar.resolve(strict=True)
            if not resolved_keg.is_dir() or not resolved_keg.is_relative_to(cellar_root):
                raise RuntimeError(f"Homebrew keg escapes Cellar: {name} {version}")
            formula_file = resolved_keg / ".brew" / f"{name}.rb"
            if formula_file.is_symlink():
                try:
                    resolved_formula_file = formula_file.resolve(strict=True)
                except OSError:
                    resolved_formula_file = None
                if resolved_formula_file is not None and not resolved_formula_file.is_relative_to(resolved_keg):
                    resolved_formula_file = None
            else:
                resolved_formula_file = formula_file if formula_file.is_file() else None
            records.append({
                "name": name,
                "full_name": full_name,
                "tap": tap,
                "version": version,
                "license": license_value,
                "homepage": homepage,
                "stable_source_url": stable_url,
                "stable_source_checksum": stable_checksum,
                "dependencies": dependencies,
                "keg": resolved_keg,
                "formula_file": resolved_formula_file,
                "formula_id": f"{tap or 'unknown'}/{full_name}@{version}",
            })
            if len(records) > MAX_FORMULAE:
                raise RuntimeError("installed Homebrew formula version count exceeds safety limit")
    return records


def _slug(value):
    return re.sub(r"[^A-Za-z0-9._-]+", "_", value).strip("._")[:120] or "formula"


def _formula_file_record(formula, stage, copied):
    formula_id = formula["formula_id"]
    if formula_id in copied:
        return copied[formula_id]
    source = formula["formula_file"]
    if source is None or not source.is_file() or source.stat().st_size > 1024 * 1024:
        copied[formula_id] = None
        return None
    safe = _slug(formula_id) + "-" + hashlib.sha256(formula_id.encode("utf-8")).hexdigest()[:10] + ".rb"
    destination = stage / "formulae" / safe
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    record = {"path": f"formulae/{safe}", "sha256": _sha256(destination)}
    copied[formula_id] = record
    return record


def _inventory_candidates(formulae, basenames):
    found = {basename: [] for basename in basenames}
    entries = 0
    hashed_files = {}
    hashed_bytes = 0
    for formula in formulae:
        keg = formula["keg"]
        for directory, subdirs, files in os.walk(keg, followlinks=False):
            subdirs[:] = [name for name in subdirs if not (Path(directory) / name).is_symlink()]
            entries += len(subdirs) + len(files)
            if entries > MAX_CELLAR_ENTRIES:
                raise RuntimeError("Homebrew Cellar entry count exceeds safety limit")
            for name in files:
                if name not in found:
                    continue
                path = Path(directory) / name
                try:
                    resolved = path.resolve(strict=True)
                    size = resolved.stat().st_size
                except OSError:
                    continue
                if not resolved.is_file() or not resolved.is_relative_to(keg):
                    continue
                cached = hashed_files.get(resolved)
                if cached is not None:
                    digest, hash_status = cached
                elif size > MAX_HASH_BYTES:
                    digest, hash_status = None, "per_file_limit"
                    hashed_files[resolved] = (digest, hash_status)
                elif hashed_bytes + size > MAX_TOTAL_HASH_BYTES:
                    digest, hash_status = None, "aggregate_limit"
                    hashed_files[resolved] = (digest, hash_status)
                else:
                    digest, hash_status = _sha256(resolved), "computed"
                    hashed_bytes += size
                    hashed_files[resolved] = (digest, hash_status)
                found[name].append({
                    "formula_id": formula["formula_id"],
                    "source_path": path.relative_to(keg).as_posix(),
                    "source_size_bytes": size,
                    "source_sha256": digest,
                    "source_hash_status": hash_status,
                    "formula": formula,
                })
                if len(found[name]) > MAX_CANDIDATES_PER_IMAGE:
                    raise RuntimeError(f"too many exact-name Homebrew candidates for image: {name}")
    return found, hashed_bytes


def build_inventory(dependency_manifest, brew_info_path, brew_prefix, stage):
    """Create a bounded, conservative candidate map for packaged Mach-O images."""
    dependency_data, _ = _read_json(dependency_manifest, "macOS dependency manifest")
    images = _image_records(dependency_data)
    brew_info, brew_info_raw = _read_json(brew_info_path, "Homebrew info inventory")
    prefix = Path(brew_prefix).resolve(strict=True)
    cellar = prefix / "Cellar"
    if not cellar.is_dir():
        raise RuntimeError("Homebrew Cellar directory is absent")
    formulae = _formula_records(brew_info, cellar)
    candidates, hashed_bytes = _inventory_candidates(formulae, {image["basename"] for image in images})

    stage = Path(stage).resolve(strict=False)
    if stage.exists():
        raise RuntimeError(f"provenance stage already exists: {stage}")
    stage.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{stage.name}-", dir=stage.parent))
    try:
        (temporary / "BREW_INFO.json").write_bytes(brew_info_raw)
        copied_formulae = {}
        image_output = []
        for image in images:
            matches = candidates[image["basename"]]
            owner_ids = sorted({item["formula_id"] for item in matches})
            status = "unique_formula" if len(owner_ids) == 1 else "ambiguous" if owner_ids else "unmapped"
            match_records = []
            for item in sorted(matches, key=lambda value: (value["formula_id"], value["source_path"])):
                formula = item["formula"]
                match_records.append({
                    "formula": formula["name"],
                    "full_name": formula["full_name"],
                    "tap": formula["tap"],
                    "version": formula["version"],
                    "license_metadata": formula["license"],
                    "homepage": formula["homepage"],
                    "stable_source_url": formula["stable_source_url"],
                    "stable_source_checksum": formula["stable_source_checksum"],
                    "dependencies": formula["dependencies"],
                    "source_path_within_keg": item["source_path"],
                    "source_size_bytes": item["source_size_bytes"],
                    "source_sha256": item["source_sha256"],
                    "source_hash_status": item["source_hash_status"],
                    "formula_file": _formula_file_record(formula, temporary, copied_formulae),
                })
            image_output.append({
                "file": image["file"],
                "sha256": image["sha256"],
                "exact_basename_matches": len(matches),
                "match_status": status,
                "match_method": "exact basename among files in installed Homebrew Cellar kegs",
                "candidates": match_records,
            })
        result = {
            "schema_version": 1,
            "scope": (
                "Informational Homebrew owner candidates for bundled Mach-O images, matched by exact filename. "
                "Ambiguous and unmapped records require review; this does not prove binary correspondence, "
                "complete corresponding source, applicable license terms, or legal compliance."
            ),
            "homebrew_info_sha256": hashlib.sha256(brew_info_raw).hexdigest(),
            "installed_formula_versions_examined": len(formulae),
            "source_hash_bytes": hashed_bytes,
            "images": sorted(image_output, key=lambda item: item["file"]),
        }
        serialized = json.dumps(result, indent=2) + "\n"
        if len(serialized.encode("utf-8")) > MAX_JSON_BYTES:
            raise RuntimeError("macOS Homebrew provenance output exceeds size limit")
        output = temporary / "MACOS_HOMEBREW_PROVENANCE.json"
        output.write_text(serialized, encoding="utf-8")
        temporary.rename(stage)
        return result
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependencies", required=True, type=Path, help="MACOS_DEPENDENCIES.json")
    parser.add_argument("--brew-info", required=True, type=Path, help="brew info --json=v1 --installed output")
    parser.add_argument("--brew-prefix", required=True, type=Path, help="brew --prefix")
    parser.add_argument("--stage", required=True, type=Path, help="new output directory inside the package stage")
    args = parser.parse_args()
    result = build_inventory(args.dependencies, args.brew_info, args.brew_prefix, args.stage)
    counts = {name: sum(image["match_status"] == name for image in result["images"])
              for name in ("unique_formula", "ambiguous", "unmapped")}
    print("Homebrew provenance candidates: "
          f"{counts['unique_formula']} unique-formula, {counts['ambiguous']} ambiguous, "
          f"{counts['unmapped']} unmapped.")


if __name__ == "__main__":
    main()
