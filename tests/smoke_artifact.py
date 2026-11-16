"""Smoke-test a packaged humtrace binary against a synthetic 60 Hz WAV."""
import json
import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


def main() -> int:
    exe = sys.argv[1]
    with tempfile.TemporaryDirectory() as tmp:
        wav_path = Path(tmp) / "tone.wav"
        out_path = Path(tmp) / "report.json"
        rate = 8000
        with wave.open(str(wav_path), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(rate)
            w.writeframes(b"".join(
                struct.pack("<h", int(8000 * math.sin(2 * math.pi * 60 * n / rate)))
                for n in range(rate * 2)))
        res = subprocess.run([exe, str(wav_path), "--json", str(out_path)],
                             capture_output=True, text=True, timeout=60)
        if res.returncode != 0:
            print(res.stdout, res.stderr, sep="\n")
            return 1
        report = json.loads(out_path.read_text(encoding="utf-8"))
        if not report:
            print("empty JSON report")
            return 1
        if "60" not in res.stdout:
            print("expected 60 Hz peak in output:\n" + res.stdout)
            return 1
    print("smoke OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
