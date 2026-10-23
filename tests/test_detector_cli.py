"""Effective detector settings must survive CLI parsing and both report paths."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave

exe = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="humtrace-detector-cli-") as temporary:
    root = Path(temporary)
    source = root / "settings.wav"
    with wave.open(str(source), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(4096)
        wav.writeframes(b"".join(struct.pack("<h", 1000 if i % 64 < 32 else -1000)
                                 for i in range(8192 + 17)))
    options = ["--frame-size", "2048", "--hop-size", "512", "--keep-dc",
               "--band-min-hz", "40", "--band-max-hz", "500",
               "--peak-separation-hz", "3", "--floor-guard-bins", "1",
               "--floor-radius-bins", "6", "--harmonic-tolerance-hz", "2.5",
               "--peak-min-dbfs", "-75.25", "--peak-prominence-db", "8.5",
               "--harmonic-min-dbfs", "-55.25", "--harmonic-prominence-db", "9.5",
               "--track-min-frames", "1", "--max-peaks", "4", "--psd-remove-mean"]
    outputs = []
    for mode in ([], ["--stream"]):
        output = root / ("stream.json" if mode else "retained.json")
        result = subprocess.run([str(exe), str(source), "--json", str(output), "--quiet", *options, *mode],
                                capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        outputs.append(json.loads(output.read_text(encoding="utf-8")))
    assert outputs[0] == outputs[1], "custom configuration differs between storage paths"
    analysis = outputs[0]["settings"]
    assert analysis["frame_size_samples"] == 2048 and analysis["hop_size_samples"] == 512, analysis
    # The irregular tail must remain anchored to the original last input sample.
    assert outputs[0]["channels"][0]["frames"][-1]["start_sample"] == 8192 + 17 - 2048
    for invalid in (["--frame-size", "2000"], ["--hop-size", "999999999999999999999999"],
                    ["--band-min-hz", "nan"], ["--floor-radius-bins", "0"],
                    ["--window", "blackman"], ["--map", ""]):
        result = subprocess.run([str(exe), str(root / "missing.wav"), *invalid], capture_output=True, text=True)
        assert result.returncode != 0 and "could not open" not in result.stderr, (invalid, result.stderr)
print("Detector CLI: effective settings, irregular tail, exact streaming parity and upfront rejection passed.")
