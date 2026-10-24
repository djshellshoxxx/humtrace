"""Run extracted package executables without the build toolchain's search path."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import wave

MAX_SOURCE_FILE_BYTES = 1024


def validate_source_commit(expected, source_file):
    """Require bounded source metadata to match the CI checkout SHA exactly."""
    if not re.fullmatch(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})", expected or ""):
        raise RuntimeError("expected source commit must be 40 or 64 hexadecimal characters")
    source_file = Path(source_file)
    try:
        with source_file.open("rb") as source:
            raw_content = source.read(MAX_SOURCE_FILE_BYTES + 1)
        if len(raw_content) > MAX_SOURCE_FILE_BYTES:
            raise RuntimeError(f"source commit file is too large: {source_file}")
        content = raw_content.decode("utf-8").strip()
    except RuntimeError:
        raise
    except (OSError, UnicodeError) as error:
        raise RuntimeError(f"cannot read source commit file {source_file}: {error}") from error
    if content != expected:
        raise RuntimeError(f"source commit file does not match expected source commit: {source_file}")
    return content


def run_smoke(cli, gui):
    cli, gui = Path(cli).resolve(), Path(gui).resolve()
    environment = dict(os.environ)
    for key in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
        environment.pop(key, None)
    if os.name == "nt":
        windows = Path(environment["WINDIR"])
        environment["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
        environment.pop("QT_QPA_PLATFORM", None)  # Exercise the packaged native Windows plugin.
    else:
        environment["PATH"] = "/usr/bin:/bin"
        environment["QT_QPA_PLATFORM"] = "offscreen"
    with tempfile.TemporaryDirectory(prefix="humtrace-package-") as temporary:
        root = Path(temporary)
        source = root / "測定-π.wav"
        output = root / "report.json"
        with wave.open(str(source), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(4096)
            wav.writeframes(b"".join(struct.pack("<h", round(9000 * math.sin(2 * math.pi * 60 * i / 4096)))
                                     for i in range(16384)))
        subprocess.run([str(cli), str(source), "--json", str(output), "--quiet"],
                       cwd=root, env=environment, check=True, timeout=30)
        report = json.loads(output.read_text(encoding="utf-8"))
        assert report["channels"][0]["tracks"], report
        version = subprocess.run([str(gui), "--version"], cwd=root, env=environment,
                                 capture_output=True, text=True, encoding="utf-8", check=True, timeout=30)
        assert "HumTrace" in version.stdout, version
        process = subprocess.Popen([str(gui), str(source)], cwd=root, env=environment,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            _, error = process.communicate(timeout=5)
            raise RuntimeError(f"packaged GUI exited before smoke interval: {process.returncode}: {error!r}")
        except subprocess.TimeoutExpired:
            process.terminate()
            process.communicate(timeout=10)
    print("Package smoke passed: isolated PATH, UTF-8 input, CLI result, GUI initialization and 5 s event loop.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--gui", type=Path, required=True)
    parser.add_argument("--expected-source-sha")
    parser.add_argument("--source-file", type=Path)
    args = parser.parse_args(argv)
    if (args.expected_source_sha is None) != (args.source_file is None):
        parser.error("--expected-source-sha and --source-file must be supplied together")
    if args.expected_source_sha is not None:
        validate_source_commit(args.expected_source_sha, args.source_file)
    run_smoke(args.cli, args.gui)


if __name__ == "__main__":
    main()
