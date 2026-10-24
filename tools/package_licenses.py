"""Copy MSYS2 package license files for the bundled runtime DLLs.

This creates provenance evidence for a package inventory. It does not determine
which license option applies or establish legal compliance.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess

MAX_SPDX_BYTES = 16 * 1024 * 1024
MAX_SPDX_CANDIDATES = 4


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_runtime_dependencies(manifest_path, stage):
    """Return validated staged DLL entries from DEPENDENCIES.json.

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
        if relative.suffix.lower() != ".dll":
            continue
        name = relative.as_posix()
        key = name.casefold()
        if key in dependencies:
            raise RuntimeError(f"duplicate staged runtime DLL in dependency manifest: {name}")
        staged_file = stage.joinpath(*relative.parts)
        if not staged_file.is_file():
            raise RuntimeError(f"manifest runtime DLL is absent from stage: {name}")
        if not staged_file.resolve().is_relative_to(stage.resolve()):
            raise RuntimeError(f"staged runtime DLL escapes package stage: {name}")
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
    """Find package-owned notices in share/licenses or conventional license files.

    Some packages install their sole license elsewhere in the package prefix;
    ICU, for example, lists share/icu/<version>/LICENSE.
    """
    root = PurePosixPath(pacman_root)
    licenses_prefix = root / "share" / "licenses"
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
        if not path.is_absolute():
            raise RuntimeError(f"unsafe relative pacman license path for {package}: {raw_path}")
        try:
            path.relative_to(root)
        except ValueError:
            if path.name.casefold().startswith(("license", "licence", "copying", "copyright")):
                raise RuntimeError(f"license candidate is outside package prefix: {raw_path}")
            continue
        try:
            path.relative_to(licenses_prefix)
            in_license_tree = True
        except ValueError:
            in_license_tree = False
        if in_license_tree:
            paths.append(path)
        elif re.match(r"^(?:licenses?|licences?|copying|copyright)(?:$|[._ -])", path.name, re.IGNORECASE):
            paths.append(path)
    return sorted(set(paths), key=str)


def _installed_path(path, pacman_root, runtime_root):
    """Translate an MSYS2 absolute package path into the supplied install root."""
    relative = path.relative_to(PurePosixPath(pacman_root))
    return Path(runtime_root).joinpath(*relative.parts)


def _query_plugin_root(runtime, runtime_root, run, pacman, pacman_root):
    """Query Qt for its exact installed plugin root; never scan the prefix."""
    candidates = [Path(runtime) / "qmake6.exe", Path(runtime) / "qmake6"]
    qmake = next((candidate for candidate in candidates if candidate.is_file()), None)
    if qmake is None:
        raise RuntimeError("nested runtime DLLs require installed qmake6 to locate the Qt plugin root")
    output = run([str(qmake), "-query", "QT_INSTALL_PLUGINS"])
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError(f"qmake6 returned an invalid QT_INSTALL_PLUGINS path: {output!r}")
    raw = lines[0].replace("\\", "/")
    queried = PurePosixPath(raw)
    if ".." in queried.parts:
        raise RuntimeError(f"qmake6 returned an unsafe plugin root: {lines[0]}")
    if queried.is_absolute():
        try:
            relative = queried.relative_to(PurePosixPath(pacman_root))
            plugin_root = Path(runtime_root).joinpath(*relative.parts)
        except ValueError:
            plugin_root = Path(lines[0])
    elif re.match(r"^[A-Za-z]:/", raw):
        # Native Windows Qt builds can report a drive-qualified install path.
        plugin_root = Path(lines[0])
    else:
        raise RuntimeError(f"qmake6 returned a non-absolute plugin root: {lines[0]}")
    if not plugin_root.is_dir() or not plugin_root.resolve().is_relative_to(Path(runtime_root).resolve()):
        raise RuntimeError(f"Qt plugin root is absent or escapes package prefix: {plugin_root}")
    return plugin_root


def _plugin_pacman_path(relative_file, plugin_root, runtime_root, pacman_root):
    """Map staged Qt plugin layout to the same relative path below queried root."""
    plugin_root = Path(plugin_root)
    runtime_root = Path(runtime_root)
    if not plugin_root.resolve().is_relative_to(runtime_root.resolve()):
        raise RuntimeError(f"Qt plugin root escapes package prefix: {plugin_root}")
    relative = PurePosixPath(relative_file)
    if relative.is_absolute() or ".." in relative.parts:
        raise RuntimeError(f"unsafe staged plugin path: {relative_file}")
    origin = plugin_root.joinpath(*relative.parts)
    if not origin.resolve().is_relative_to(runtime_root.resolve()):
        raise RuntimeError(f"Qt plugin origin escapes package prefix: {origin}")
    if not origin.is_file():
        raise RuntimeError(f"runtime origin is missing for staged dependency: {origin}")
    plugin_posix_root = PurePosixPath(pacman_root)
    try:
        plugin_relative = plugin_root.resolve().relative_to(runtime_root.resolve())
    except ValueError as error:
        raise RuntimeError(f"Qt plugin root escapes package prefix: {plugin_root}") from error
    return plugin_posix_root.joinpath(*plugin_relative.parts, *relative.parts), origin


def _parse_qtbase_spdx_paths(output, package, pacman_root):
    """Return a bounded, package-owned QtBase SPDX file list."""
    root = PurePosixPath(pacman_root)
    sbom_prefix = root / "share" / "qt6" / "sbom"
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
            raise RuntimeError(f"unsafe QtBase SPDX path for {package}: {raw_path}")
        if not path.is_absolute():
            if path.name.startswith("qtbase-") and path.suffix == ".spdx":
                raise RuntimeError(f"unsafe relative QtBase SPDX path for {package}: {raw_path}")
            continue
        try:
            path.relative_to(root)
        except ValueError:
            if path.name.startswith("qtbase-") and path.suffix == ".spdx":
                raise RuntimeError(f"QtBase SPDX candidate is outside package prefix: {raw_path}")
            continue
        try:
            relative = path.relative_to(sbom_prefix)
        except ValueError:
            continue
        if len(relative.parts) == 1 and relative.name.startswith("qtbase-") and relative.suffix == ".spdx":
            paths.append(path)
    paths = sorted(set(paths), key=str)
    if len(paths) > MAX_SPDX_CANDIDATES:
        raise RuntimeError(f"too many QtBase SPDX candidates for {package}")
    if len(paths) != 1:
        raise RuntimeError(f"expected one installed QtBase SPDX file for {package}, found {len(paths)}")
    return paths


def collect_package_licenses(dependency_manifest, runtime, stage, run=None,
                             pacman_root="/ucrt64", pacman="pacman", plugin_root=None):
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
    nested = any("/" in dependency["file"] for dependency in dependencies)
    if nested and plugin_root is None:
        plugin_root = _query_plugin_root(runtime, runtime_root, run, pacman, pacman_root_path)
    for dependency in dependencies:
        name = dependency["file"]
        if "/" in name:
            pacman_path, origin = _plugin_pacman_path(
                name, plugin_root, runtime_root, pacman_root_path)
        else:
            pacman_path = pacman_root_path / runtime.name / name
            origin = runtime / name
        if not origin.is_file():
            raise RuntimeError(f"runtime origin is missing for staged dependency: {origin}")
        if not origin.resolve().is_relative_to(runtime_root.resolve()):
            raise RuntimeError(f"runtime DLL origin escapes package prefix: {origin}")
        if _sha256(origin) != dependency["sha256"]:
            raise RuntimeError(f"staged runtime DLL does not match installed origin: {name}")
        owner_output = run([pacman, "-Qqo", str(pacman_path)])
        package = _parse_owner(owner_output, str(pacman_path))
        if package not in planned_packages:
            version_output = run([pacman, "-Q", package])
            version = _parse_version(version_output, package)
            file_list = run([pacman, "-Ql", package])
            license_paths = _parse_license_paths(file_list, package, pacman_root_path)
            spdx_paths = []
            if "qt6-base" in package.casefold():
                spdx_paths = _parse_qtbase_spdx_paths(file_list, package, pacman_root_path)
            records = []
            for license_path in license_paths:
                source = _installed_path(license_path, pacman_root_path, runtime_root)
                if source.is_dir():
                    continue
                if not source.is_file():
                    raise RuntimeError(f"pacman-listed license file is missing for {package}: {source}")
                resolved_source = source.resolve()
                if not resolved_source.is_relative_to(runtime_root.resolve()):
                    raise RuntimeError(f"pacman-listed license file escapes package prefix for {package}: {source}")
                try:
                    relative = license_path.relative_to(pacman_root_path / "share" / "licenses")
                except ValueError:
                    # Preserve the package-relative location for license files
                    # installed outside the conventional shared license tree.
                    relative = license_path.relative_to(pacman_root_path)
                else:
                    if relative.parts and relative.parts[0] == package:
                        relative = PurePosixPath(*relative.parts[1:])
                target_relative = PurePosixPath("licenses") / package / relative
                if any(record["relative"] == target_relative for record in records):
                    raise RuntimeError(f"duplicate package license destination for {package}: {target_relative}")
                records.append({"source": source, "relative": target_relative,
                                "origin": license_path.as_posix(), "sha256": _sha256(source)})
            if not records:
                raise RuntimeError(f"package {package} has no installed license files")
            spdx_records = []
            for spdx_path in spdx_paths:
                source = _installed_path(spdx_path, pacman_root_path, runtime_root)
                if not source.is_file():
                    raise RuntimeError(f"pacman-listed QtBase SPDX file is missing for {package}: {source}")
                if not source.resolve().is_relative_to(runtime_root.resolve()):
                    raise RuntimeError(f"pacman-listed QtBase SPDX file escapes package prefix for {package}: {source}")
                if source.stat().st_size <= 0 or source.stat().st_size > MAX_SPDX_BYTES:
                    raise RuntimeError(f"QtBase SPDX file has invalid size for {package}: {source}")
                relative = spdx_path.relative_to(pacman_root_path)
                target_relative = PurePosixPath("licenses") / package / relative
                spdx_records.append({"source": source, "relative": target_relative,
                                    "origin": spdx_path.as_posix(), "sha256": _sha256(source)})
            planned_packages[package] = {"version": version, "license_records": records,
                                         "spdx_records": spdx_records}

        dependency["origin"] = str(pacman_path)
        dependency["package"] = package
        dependency["version"] = planned_packages[package]["version"]
        dependency["license_files"] = [
            {"path": record["relative"].as_posix(), "origin": record["origin"],
             "sha256": record["sha256"]}
            for record in planned_packages[package]["license_records"]
        ]
        dependency["spdx_file"] = next((
            {"path": record["relative"].as_posix(), "origin": record["origin"],
             "sha256": record["sha256"]}
            for record in planned_packages[package]["spdx_records"]), None)

    licenses_dir = stage / "licenses"
    output_path = stage / "DEPENDENCY_LICENSES.json"
    if licenses_dir.exists() or output_path.exists():
        raise RuntimeError("license inventory output already exists in stage; use a clean stage")

    result = {
        "schema_version": 1,
        "pacman_root": str(pacman_root_path),
        "dependencies": dependencies,
        "notices": [],
    }
    for package in sorted(planned_packages):
        for kind in ("license", "spdx"):
            key = "license_records" if kind == "license" else "spdx_records"
            for record in planned_packages[package][key]:
                destination = stage.joinpath(*record["relative"].parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(record["source"], destination)
                result["notices"].append({"kind": kind, "package": package,
                    "version": planned_packages[package]["version"],
                    "path": record["relative"].as_posix(), "origin": record["origin"],
                    "sha256": record["sha256"]})
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
