"""Deploy the native QtWidgets bundle, audit relocation, then sign inside out.

Run on macOS against a freshly built app. Only Cocoa and the offscreen smoke-test
platform plugin are included. Native CI remains the loading/signature authority.
Qt: https://doc.qt.io/qt-6/macos-deployment.html
Apple: https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/Procedures/Procedures.html
"""
import argparse
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess

PLATFORM_PLUGINS = ("libqcocoa.dylib", "libqoffscreen.dylib")
MAX_BINARIES = 256
MAX_FILES = 20000
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
    info = {"imports": [], "rpaths": [], "identity": None}
    command = None
    for line in output.splitlines():
        match = re.match(r"\s*cmd (LC_\S+)\s*$", line)
        if match:
            command = match.group(1)
            continue
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


def package_app(app, qt_prefix, brew_prefix):
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

    def inspect(binary):
        return parse_load_commands(run(["otool", "-l", binary]))

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
    required_arch = platform.machine()
    for binary in binaries:
        arches = run(["lipo", "-archs", binary]).split()
        if required_arch not in arches:
            raise RuntimeError(f"missing native architecture {required_arch}: {binary}")
        command = relocation_command(app, binary, inspect(binary))
        if command:
            run(command)
    audit_dependencies(app, executable, binaries, inspect)
    for target in signing_order(app, binaries):
        run(["codesign", "--force", "--sign", "-", "--timestamp=none", target])
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", app])
    print(f"Verified {len(binaries)} native Mach-O images; Cocoa/offscreen plugins only; ad-hoc signature valid.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--qt-prefix", required=True, type=Path)
    parser.add_argument("--brew-prefix", required=True, type=Path)
    args = parser.parse_args()
    package_app(args.app, args.qt_prefix, args.brew_prefix)


if __name__ == "__main__":
    main()
