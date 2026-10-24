"""Copy MSYS2 package license files for the bundled root runtime DLLs.

This creates provenance evidence for a package inventory. It does not determine
which license option applies or establish legal compliance.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_runtime_dependencies(manifest_path, stage):
    """Return validated root-level DLL entries from DEPENDENCIES.json.

    Entries in `imports` are names only; Windows system imports are not staged
    files and therefore are intentionally not queried for package ownership.
    """
    manifest_path, stage = Path(manifest_path), Path(stage)
    try:
        entries = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot read dependency manifest {manifest_path}: {error}") from error
    if not isinstance(entries, list):
        raise RuntimeError("DEPENDENCIES.json must contain a list of binary records")

    dependencies = {}
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("file"), str):
            raise RuntimeError("invalid binary record in DEPENDENCIES.json")
        relative = PurePosixPath(entry["file"])
        if relative.is_absolute() or ".." in relative.parts:
            raise RuntimeError(f"invalid staged binary path: {entry['file']}")
        if len(relative.parts) != 1 or relative.suffix.lower() != ".dll":
            continue
        name = relative.name
        key = name.lower()
        if key in dependencies:
            raise RuntimeError(f"duplicate root runtime DLL in dependency manifest: {name}")
        staged_file = stage / name
        if not staged_file.is_file():
            raise RuntimeError(f"manifest runtime DLL is absent from stage: {name}")
        actual_hash = _sha256(staged_file)
        expected_hash = entry.get("sha256")
        if expected_hash and expected_hash != actual_hash:
            raise RuntimeError(f"staged runtime DLL hash differs from dependency manifest: {name}")
        dependencies[key] = {"file": name, "sha256": actual_hash}
    if not dependencies:
        raise RuntimeError("DEPENDENCIES.json contains no root runtime DLLs to inventory")
    return [dependencies[key] for key in sorted(dependencies)]


def _parse_owner(output, path):
    packages = [line.strip() for line in output.splitlines() if line.strip()]
    if len(packages) != 1 or any(character.isspace() for character in packages[0]):
        raise RuntimeError(f"could not resolve one pacman owner for {path}: {output.strip()!r}")
    return packages[0]


def _parse_version(output, expected_package):
    fields = output.strip().split()
    if len(fields) != 2 or fields[0] != expected_package:
        raise RuntimeError(f"unexpected pacman version response for {expected_package}: {output.strip()!r}")
    return fields[1]


def _parse_license_paths(output, package, pacman_root):
    """Parse `pacman -Ql`; return license paths below share/licenses only."""
    prefix = PurePosixPath(pacman_root) / "share" / "licenses"
    paths = []
    for line in output.splitlines():
        fields = line.split(maxsplit=1)
        if len(fields) != 2:
            continue
        owner, raw_path = fields
        if owner != package:
            raise RuntimeError(f"pacman -Ql returned unexpected owner {owner!r} for {package!r}")
        path = PurePosixPath(raw_path)
        if ".." in path.parts:
            raise RuntimeError(f"unsafe pacman license path for {package}: {raw_path}")
        try:
            path.relative_to(prefix)
        except ValueError:
            continue
        paths.append(path)
    return sorted(set(paths), key=str)


def _installed_path(path, pacman_root, runtime_root):
    """Translate an MSYS2 absolute package path into the supplied install root."""
    relative = path.relative_to(PurePosixPath(pacman_root))
    return Path(runtime_root).joinpath(*relative.parts)


def collect_package_licenses(dependency_manifest, runtime, stage, run=None,
                             pacman_root="/ucrt64", pacman="pacman"):
    """Inventory owners and copy their installed license files into a package stage.

    `runtime` is the installed runtime `bin` directory on disk. `pacman_root`
    is its MSYS2 path prefix used for package database queries.
    """
    if run is None:
        def run(command):
            try:
                return subprocess.check_output(command, text=True, encoding="utf-8",
                                               stderr=subprocess.STDOUT)
            except subprocess.CalledProcessError as error:
                detail = error.output.strip() if error.output else str(error)
                raise RuntimeError(f"command failed ({' '.join(command)}): {detail}") from error

    runtime, stage = Path(runtime), Path(stage)
    pacman_root_path = PurePosixPath(pacman_root)
    if not pacman_root_path.is_absolute() or ".." in pacman_root_path.parts:
        raise RuntimeError(f"invalid MSYS2 package root: {pacman_root}")
    if not runtime.is_dir() or not stage.is_dir():
        raise RuntimeError("runtime and stage must be existing directories")

    dependencies = read_runtime_dependencies(dependency_manifest, stage)
    runtime_root = runtime.parent
    planned_packages = {}
    for dependency in dependencies:
        name = dependency["file"]
        pacman_path = pacman_root_path / runtime.name / name
        origin = runtime / name
        if not origin.is_file():
            raise RuntimeError(f"runtime origin is missing for staged dependency: {origin}")
        if _sha256(origin) != dependency["sha256"]:
            raise RuntimeError(f"staged runtime DLL does not match installed origin: {name}")
        owner_output = run([pacman, "-Qqo", str(pacman_path)])
        package = _parse_owner(owner_output, str(pacman_path))
        if package not in planned_packages:
            version_output = run([pacman, "-Q", package])
            version = _parse_version(version_output, package)
            file_list = run([pacman, "-Ql", package])
            license_paths = _parse_license_paths(file_list, package, pacman_root_path)
            records = []
            for license_path in license_paths:
                source = _installed_path(license_path, pacman_root_path, runtime_root)
                if source.is_dir():
                    continue
                if not source.is_file():
                    raise RuntimeError(f"pacman-listed license file is missing for {package}: {source}")
                relative = license_path.relative_to(pacman_root_path / "share" / "licenses")
                if relative.parts and relative.parts[0] == package:
                    relative = PurePosixPath(*relative.parts[1:])
                target_relative = PurePosixPath("licenses") / package / relative
                records.append({"source": source, "relative": target_relative,
                                "sha256": _sha256(source)})
            if not records:
                raise RuntimeError(f"package {package} has no installed license files under share/licenses")
            planned_packages[package] = {"version": version, "license_records": records}

        dependency["origin"] = str(pacman_path)
        dependency["package"] = package
        dependency["version"] = planned_packages[package]["version"]
        dependency["license_files"] = [
            {"path": record["relative"].as_posix(), "sha256": record["sha256"]}
            for record in planned_packages[package]["license_records"]
        ]

    licenses_dir = stage / "licenses"
    output_path = stage / "DEPENDENCY_LICENSES.json"
    if licenses_dir.exists() or output_path.exists():
        raise RuntimeError("license inventory output already exists in stage; use a clean stage")

    result = {
        "schema_version": 1,
        "pacman_root": str(pacman_root_path),
        "dependencies": dependencies,
    }
    for package in sorted(planned_packages):
        for record in planned_packages[package]["license_records"]:
            destination = stage.joinpath(*record["relative"].parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(record["source"], destination)
    output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", required=True, type=Path,
                        help="installed UCRT64 runtime bin directory")
    parser.add_argument("--stage", required=True, type=Path,
                        help="package stage containing DEPENDENCIES.json")
    parser.add_argument("--pacman-root", default="/ucrt64",
                        help="MSYS2 path prefix used in pacman queries")
    parser.add_argument("--pacman", default="pacman")
    args = parser.parse_args()
    collect_package_licenses(args.stage / "DEPENDENCIES.json", args.runtime, args.stage,
                             pacman_root=args.pacman_root, pacman=args.pacman)


if __name__ == "__main__":
    main()
