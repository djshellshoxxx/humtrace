"""Stage a Windows runtime dependency closure before archiving it.

The PE inspector is objdump from the native MinGW toolchain. This intentionally
does not assume that windeployqt finds compiler or transitive runtime DLLs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import zipfile


def collect_dependencies(stage, runtime, system, inspect):
    """Copy imported runtime DLLs and reject imports that cannot be resolved."""
    stage, runtime, system = map(Path, (stage, runtime, system))
    available = {p.name.lower(): p for p in runtime.glob("*") if p.is_file()}
    system_names = {p.name.lower() for p in system.glob("*") if p.is_file()}
    pending = sorted(p for p in stage.rglob("*") if p.suffix.lower() in (".exe", ".dll"))
    visited, manifest = set(), []
    while pending:
        binary = pending.pop(0)
        if binary in visited:
            continue
        visited.add(binary)
        imports = sorted(set(inspect(binary)), key=str.lower)
        for name in imports:
            if Path(name).name != name or "/" in name or "\\" in name:
                raise RuntimeError(f"invalid PE import name: {name}")
            key = name.lower()
            staged = {p.name.lower(): p for p in stage.glob("*") if p.is_file()}
            if key in staged:
                pending.append(staged[key])
            elif key in available:
                destination = stage / available[key].name
                shutil.copy2(available[key], destination)
                pending.append(destination)
            elif key not in system_names and not key.startswith(("api-ms-win-", "ext-ms-win-")):
                raise RuntimeError(f"unresolved dependency {name} imported by {binary.name}")
        manifest.append({"file": binary.relative_to(stage).as_posix(), "imports": imports,
                         "sha256": hashlib.sha256(binary.read_bytes()).hexdigest()})
    return sorted(manifest, key=lambda item: item["file"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--objdump", default="objdump")
    args = parser.parse_args()

    def inspect(binary):
        output = subprocess.check_output([args.objdump, "-p", str(binary)], text=True,
                                         encoding="utf-8")
        return re.findall(r"DLL Name:\s*(\S+)", output)

    windows = Path(os.environ["WINDIR"])
    manifest = collect_dependencies(args.stage, args.runtime, windows / "System32", inspect)
    # Assert the known failure condition separately, so an empty inspector cannot pass.
    for name in ("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll"):
        if not (args.stage / name).is_file():
            raise RuntimeError(f"required Qt runtime absent: {name}")
    (args.stage / "DEPENDENCIES.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if args.archive is None:
        return
    with zipfile.ZipFile(args.archive, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(args.stage.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(args.stage))


if __name__ == "__main__":
    main()
