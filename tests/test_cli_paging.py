"""Dense ordinary CLI analysis pages automatically without changing measurements."""
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave

exe = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="humtrace-auto-page-") as temporary:
    root = Path(temporary)
    source = root / "dense.wav"
    with wave.open(str(source), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(4096)
        wav.writeframes(b"".join(struct.pack("<h", round(10000 * math.sin(2 * math.pi * 256 * i / 4096)))
                                 for i in range(65536 + 7)))
    common = [str(exe), str(source), "--frame-size", "16", "--hop-size", "16", "--quiet"]
    paged = subprocess.run([*common, "--json", str(root / "paged.json")], capture_output=True, text=True, timeout=60)
    assert paged.returncode == 0, paged.stderr
    assert "Storage: disk-backed" in paged.stdout, "dense ordinary CLI did not use bounded storage"
    retained = subprocess.run([*common, "--json", str(root / "retained.json"), "--csv", str(root / "tracks.csv")],
                              capture_output=True, text=True, timeout=60)
    assert retained.returncode == 0, retained.stderr
    assert json.loads((root / "paged.json").read_text()) == json.loads((root / "retained.json").read_text())
    assert "Channel 1 measured frames: 4097" in paged.stdout, paged.stdout
    assert not list(root.glob("*.humtrace-tmp-*")) and not list(root.glob(".humtrace-stream-*"))
print("CLI auto-paging: dense settings preserve complete report values and frame counts.")
