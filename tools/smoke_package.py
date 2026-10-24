"""Run extracted package executables without the build toolchain's search path."""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cli", type=Path, required=True)
parser.add_argument("--gui", type=Path, required=True)
args = parser.parse_args()
cli, gui = args.cli.resolve(), args.gui.resolve()
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
