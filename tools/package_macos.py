"""Deploy the native QtWidgets bundle, audit relocation, then sign inside out.

Run on macOS against a freshly built app. Only Cocoa and the offscreen smoke-test
platform plugin are included. Native CI remains the loading/signature authority.
Qt: https://doc.qt.io/qt-6/macos-deployment.html
Apple: https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/Procedures/Procedures.html
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import tempfile

PLATFORM_PLUGINS = ("libqcocoa.dylib", "libqoffscreen.dylib")
MAX_BINARIES = 256
MAX_FILES = 20000
MAX_LOAD_COMMANDS = 4096
MACHO_MAGICS = {bytes.fromhex(value) for value in
                ("feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}
LOAD_COMMANDS = {"LC_LOAD_DYLIB", "LC_LOAD_WEAK_DYLIB", "LC_REEXPORT_DYLIB",
                 "LC_LOAD_UPWARD_DYLIB", "LC_LAZY_LOAD_DYLIB"}


def run_command(command):
    command = list(map(str, command))
    try:
        return subprocess.check_output(command, text=True, encoding="utf-8",
                                       stderr=subprocess.STDOUT, timeout=180)
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"command failed ({' '.join(command)}): {error.output.strip()}") from error


def parse_load_commands(output):
    info = {"imports": [], "rpaths": [], "identity": None, "build_versions": []}
    command = None
    build_version = None
    count = 0
    legacy_platforms = {"LC_VERSION_MIN_MACOSX": "MACOS", "LC_VERSION_MIN_IPHONEOS": "IOS",
                        "LC_VERSION_MIN_TVOS": "TVOS", "LC_VERSION_MIN_WATCHOS": "WATCHOS"}
    for line in output.splitlines():
        match = re.match(r"\s*cmd (LC_\S+)\s*$", line)
        if match:
            count += 1
            if count > MAX_LOAD_COMMANDS:
                raise RuntimeError("Mach-O load-command count limit exceeded")
            command = match.group(1)
            build_version = None
            if command == "LC_BUILD_VERSION" or command in legacy_platforms:
                build_version = {"platform": legacy_platforms.get(command), "minos": None, "sdk": None}
                info["build_versions"].append(build_version)
            continue
        if build_version is not None:
            match = re.match(r"\s*(platform|minos|version|sdk) (\S+)\s*$", line)
            if match:
                key, value = match.groups()
                if key == "platform":
                    build_version[key] = "MACOS" if value.upper() in ("1", "MACOS") else value.upper()
                elif key == "minos" or (key == "version" and command in legacy_platforms):
                    build_version["minos"] = value
                elif key == "sdk":
                    build_version[key] = value
        match = re.match(r"\s*(?:name|path) (.+) \(offset \d+\)\s*$", line)
        if not match:
            continue
        if command == "LC_RPATH":
            info["rpaths"].append(match.group(1))
        elif command == "LC_ID_DYLIB":
            info["identity"] = match.group(1)
        elif command in LOAD_COMMANDS:
            info["imports"].append(match.group(1))
    return info


def _version_parts(value, field):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9]{1,5}(?:\.[0-9]{1,3}){0,2}", value):
        raise RuntimeError(f"missing or invalid {field}: {value!r}")
    parts = tuple(map(int, value.split(".")))
    parts += (0,) * (3 - len(parts))
    if parts[0] > 65535 or parts[1] > 255 or parts[2] > 255:
        raise RuntimeError(f"out-of-range {field}: {value}")
    return parts


def validate_minimum_os(info, minimum_os, binary):
    baseline = _version_parts(minimum_os, "declared minimum OS")
    records = info.get("build_versions", [])
    if len(records) != 1:
        raise RuntimeError(f"missing or ambiguous minimum OS metadata for selected architecture: {binary}")
    record = records[0]
    if record.get("platform") != "MACOS":
        raise RuntimeError(f"non-macOS build platform {record.get('platform')!r}: {binary}")
    minimum = _version_parts(record.get("minos"), "image minimum OS")
    _version_parts(record.get("sdk"), "image SDK")
    if minimum == (0, 0, 0):
        raise RuntimeError(f"invalid image minimum OS 0: {binary}")
    if minimum > baseline:
        raise RuntimeError(f"image minimum OS {record['minos']} exceeds declared {minimum_os}: {binary}")
    return dict(record)


def stage_platform_plugins(app, qt_prefix, plugins_root=None):
    root = Path(plugins_root) if plugins_root is not None else Path(qt_prefix) / "plugins"
    destination = Path(app) / "Contents/PlugIns/platforms"
    sources = [root / "platforms" / name for name in PLATFORM_PLUGINS]
    for source in sources:
        if not source.is_file():
            raise RuntimeError(f"required Qt platform plugin is absent: {source}")
    destination.mkdir(parents=True, exist_ok=True)
    copied = []
    for source in sources:
        target = destination / source.name
        if target.exists() or target.is_symlink():
            raise RuntimeError(f"plugin destination already exists; use a fresh app: {target}")
        shutil.copy2(source, target)
        target.chmod(target.stat().st_mode | 0o200)
        copied.append(target)
    return copied


def deployment_command(app, qt_prefix, brew_prefix, plugins, library_paths=()):
    paths = dict.fromkeys([Path(qt_prefix) / "lib", Path(brew_prefix) / "lib", *map(Path, library_paths)])
    return [str(Path(qt_prefix) / "bin/macdeployqt"), str(app), "-no-plugins", "-no-codesign",
            "-no-strip", "-verbose=2", *["-libpath=" + str(path) for path in paths],
            *["-executable=" + str(path) for path in plugins]]


def validate_symlinks(app):
    app = Path(app).resolve()
    for count, path in enumerate(app.rglob("*"), 1):
        if count > MAX_FILES:
            raise RuntimeError("app file-count limit exceeded")
        if path.is_symlink() and (not path.exists() or not path.resolve().is_relative_to(app)):
            raise RuntimeError(f"bundle symlink escapes or is broken: {path}")


def macho_files(app):
    validate_symlinks(app)
    binaries = []
    for path in sorted(Path(app).rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as source:
            if source.read(4) in MACHO_MAGICS:
                binaries.append(path)
        if len(binaries) > MAX_BINARIES:
            raise RuntimeError("app Mach-O count limit exceeded")
    return binaries


def _expand(path, binary, executable):
    for prefix, base in (("@loader_path", binary.parent), ("@executable_path", executable.parent)):
        if path == prefix or path.startswith(prefix + "/"):
            return (base / path[len(prefix):].lstrip("/")).resolve()
    if path.startswith("/"):
        return Path(path).resolve()
    return None


def relocation_command(app, binary, info):
    frameworks = Path(app) / "Contents/Frameworks"
    local = "@loader_path/" + Path(os.path.relpath(frameworks, Path(binary).parent)).as_posix()
    arguments = []
    for old in dict.fromkeys(info["rpaths"]):
        if old != local:
            arguments.extend(["-delete_rpath", old])
    if any(name.startswith("@rpath/") for name in info["imports"]) and local not in info["rpaths"]:
        arguments.extend(["-add_rpath", local])
    return ["install_name_tool", *arguments, str(binary)] if arguments else []


def audit_dependencies(app, executable, binaries, inspect):
    app, executable = Path(app).resolve(), Path(executable).resolve()
    images = {Path(binary).resolve(): inspect(binary) for binary in binaries}
    if executable not in images:
        raise RuntimeError("app executable is not a Mach-O image")
    for binary, info in images.items():
        rpaths = info["rpaths"] + images[executable]["rpaths"]
        for rpath in rpaths:
            expanded = _expand(rpath, binary, executable)
            if expanded is None or not expanded.is_relative_to(app):
                raise RuntimeError(f"external runtime search path {rpath} in {binary}")
        for dependency in info["imports"]:
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue  # Apple system libraries can reside only in the dyld shared cache.
            if dependency.startswith("@rpath/"):
                candidates = [_expand(path, binary, executable) for path in rpaths]
                candidates = [path / dependency[len("@rpath/"):] for path in candidates if path is not None]
            else:
                candidate = _expand(dependency, binary, executable)
                candidates = [candidate] if candidate is not None else []
            if not any(path.is_file() and path.resolve() in images for path in candidates):
                raise RuntimeError(f"unresolved or external dependency {dependency} in {binary}")


def signing_order(app, binaries):
    app = Path(app)
    frameworks = {parent for binary in binaries for parent in Path(binary).parents
                  if parent != app and parent.suffix == ".framework" and parent.is_relative_to(app)}
    leaves = sorted(set(map(Path, binaries)), key=lambda path: (-len(path.parts), str(path)))
    return [*leaves, *sorted(frameworks, key=lambda path: (-len(path.parts), str(path))), app]


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def finalize_package(app, executable, binaries, minimum_os, architecture, inspect, run, cli=None):
    """Gate native metadata, sign, then record hashes of the final package bytes."""
    if len(binaries) + (cli is not None) > MAX_BINARIES:
        raise RuntimeError("package Mach-O count limit exceeded")
    _version_parts(minimum_os, "declared minimum OS")
    app, executable = Path(app).resolve(), Path(executable).resolve()
    binaries = sorted(set(Path(path).resolve() for path in binaries))
    cli = Path(cli).resolve() if cli is not None else None
    if cli is not None and cli.parent != app.parent:
        raise RuntimeError("standalone CLI must be a sibling of the app in the package directory")
    images = binaries + ([cli] if cli is not None else [])
    for binary in images:
        if not binary.is_file() or not binary.is_relative_to(app.parent):
            raise RuntimeError(f"image is absent or outside the package directory: {binary}")
        arches = run(["lipo", str(binary), "-archs"]).split()
        if architecture not in arches:
            raise RuntimeError(f"missing native architecture {architecture}: {binary}")
        info = inspect(binary)
        validate_minimum_os(info, minimum_os, binary)
        if binary == cli and any(not name.startswith(("/System/Library/", "/usr/lib/")) for name in info["imports"]):
            raise RuntimeError(f"standalone CLI has a non-system runtime dependency: {binary}")
    audit_dependencies(app, executable, binaries, inspect)
    plist_path = app / "Contents/Info.plist"
    with plist_path.open("rb") as source:
        properties = plistlib.load(source)
    properties["LSMinimumSystemVersion"] = minimum_os
    with plist_path.open("wb") as destination:
        plistlib.dump(properties, destination)
    for target in ([cli] if cli is not None else []) + signing_order(app, binaries):
        run(["codesign", "--force", "--sign", "-", "--timestamp=none", target])
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", app])
    if cli is not None:
        run(["codesign", "--verify", "--strict", "--verbose=2", cli])
    records = []
    for binary in images:
        info = inspect(binary)
        build = validate_minimum_os(info, minimum_os, binary)
        records.append({"file": binary.relative_to(app.parent).as_posix(), "sha256": _sha256(binary),
                        "imports": info["imports"], "rpaths": info["rpaths"], "identity": info.get("identity"), **build})
    result = {"schema_version": 1, "audited_architecture": architecture, "declared_minimum_os": minimum_os,
              "maximum_image_minimum_os": max((record["minos"] for record in records), key=lambda value: _version_parts(value, "minimum OS")),
              "scope": "Final signed Mach-O bytes and load commands; not a Homebrew ownership or license inventory.",
              "images": sorted(records, key=lambda record: record["file"])}
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=app.parent,
                                         prefix=".humtrace-manifest-", delete=False) as destination:
            temporary = Path(destination.name)
            json.dump(result, destination, indent=2)
            destination.write("\n")
        temporary.replace(app.parent / "MACOS_DEPENDENCIES.json")
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return result


def package_app(app, qt_prefix, brew_prefix, minimum_os, cli=None):
    _version_parts(minimum_os, "declared minimum OS")
    if platform.system() != "Darwin":
        raise RuntimeError("native macOS tools are required; this helper does not cross-package")
    app, qt_prefix, brew_prefix = map(lambda path: Path(path).resolve(), (app, qt_prefix, brew_prefix))
    if app.suffix != ".app" or not app.is_dir():
        raise RuntimeError("--app must name an existing app bundle")
    for name in ("Frameworks", "PlugIns"):
        if (app / "Contents" / name).exists():
            raise RuntimeError(f"use a freshly built app without Contents/{name}")
    with (app / "Contents/Info.plist").open("rb") as source:
        name = plistlib.load(source).get("CFBundleExecutable", "")
    if not isinstance(name, str) or not name or Path(name).name != name:
        raise RuntimeError("invalid bundle executable name")
    executable = app / "Contents/MacOS" / name

    run = run_command

    required_arch = platform.machine()

    def inspect(binary):
        return parse_load_commands(run(["otool", "-arch", required_arch, "-l", binary]))

    qmake = qt_prefix / "bin/qmake"
    plugins_root = Path(run([qmake, "-query", "QT_INSTALL_PLUGINS"]).strip())
    qt_lib = Path(run([qmake, "-query", "QT_INSTALL_LIBS"]).strip())
    plugins = stage_platform_plugins(app, qt_prefix, plugins_root)
    # macdeployqt resolves @rpath using the executable's LC_RPATHs; -libpath
    # alone only covers some lookup forms. Remove these build paths below.
    existing = inspect(executable)["rpaths"]
    for path in dict.fromkeys([qt_lib, brew_prefix / "lib"]):
        if str(path) not in existing:
            run(["install_name_tool", "-add_rpath", path, executable])
    print(run(deployment_command(app, qt_prefix, brew_prefix, plugins, [qt_lib])), end="")
    resources = app / "Contents/Resources"
    resources.mkdir(exist_ok=True)
    (resources / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=PlugIns\n", encoding="utf-8")
    binaries = macho_files(app)
    installed = {path.relative_to(app / "Contents/PlugIns").as_posix()
                 for path in (app / "Contents/PlugIns").rglob("*") if path.is_file()}
    if installed != {"platforms/" + name for name in PLATFORM_PLUGINS}:
        raise RuntimeError("unexpected Qt plugin was included in the bundle")
    forbidden = ("QtPdf", "QtSvg", "QtVirtualKeyboard", "QtVirtualKeyboardQml")
    if any((app / "Contents/Frameworks" / (name + ".framework")).exists() for name in forbidden):
        raise RuntimeError("unexpected optional Qt module in this QtWidgets-only package")
    for binary in binaries:
        command = relocation_command(app, binary, inspect(binary))
        if command:
            run(command)
    if cli is not None:
        cli = Path(cli).resolve()
        with cli.open("rb") as source:
            if source.read(4) not in MACHO_MAGICS:
                raise RuntimeError(f"standalone CLI is not a Mach-O image: {cli}")
    manifest = finalize_package(app, executable, binaries, minimum_os, required_arch, inspect, run, cli)
    print(f"Verified {len(manifest['images'])} native Mach-O images; declared macOS {minimum_os}; "
          f"maximum image minimum {manifest['maximum_image_minimum_os']}; ad-hoc signatures valid. "
          "Wrote MACOS_DEPENDENCIES.json.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--qt-prefix", required=True, type=Path)
    parser.add_argument("--brew-prefix", required=True, type=Path)
    parser.add_argument("--minimum-os", required=True, help="declared supported baseline, checked against every image")
    parser.add_argument("--cli", type=Path, help="optional standalone CLI beside the app bundle")
    args = parser.parse_args()
    package_app(args.app, args.qt_prefix, args.brew_prefix, args.minimum_os, args.cli)


if __name__ == "__main__":
    main()
