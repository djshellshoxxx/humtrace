"""Parity, quota, cancellation and cleanup tests for disk-backed analysis."""
import hashlib
import json
import math
import pathlib
import random
import signal
import struct
import subprocess
import sys
import tempfile
import time
import wave

exe = pathlib.Path(sys.argv[1]).resolve()

def run(*args):
    return subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, check=False)

with tempfile.TemporaryDirectory(prefix="humtrace-stream-") as temporary:
    root = pathlib.Path(temporary)
    rng = random.Random(42)
    for kind in ("tone", "silence", "dropouts", "noise", "crossing"):
        source = root / (kind + ".wav")
        with wave.open(str(source), "wb") as wav:
            wav.setnchannels(2)
            wav.setsampwidth(2)
            wav.setframerate(4096)
            pcm = []
            for i in range(4096 * 4 + 100):
                t = i / 4096
                for c in range(2):
                    frequency = (60 + 10 * t if c == 0 else 100 - 10 * t) if kind == "crossing" else 60 + 60*c
                    sample = 10000 * math.sin(2 * math.pi * frequency * t)
                    if kind == "silence" or (kind == "dropouts" and 1 <= t < 2):
                        sample = 0
                    if kind == "noise":
                        sample = rng.uniform(-1000, 1000)
                    pcm.append(struct.pack("<h", round(sample)))
            wav.writeframes(b"".join(pcm))
        before = hashlib.sha256(source.read_bytes()).hexdigest()
        normal, streamed = root / (kind + "-normal.json"), root / (kind + "-stream.json")
        a = run(source, "--json", normal, "--quiet")
        b = run(source, "--json", streamed, "--stream", "--memory-limit-mib", 64)
        assert a.returncode == b.returncode == 0, (a.stderr, b.stderr)
        left, right = json.loads(normal.read_text()), json.loads(streamed.read_text())
        assert left == right, kind
        assert hashlib.sha256(source.read_bytes()).hexdigest() == before
        assert not list(root.glob(".humtrace-stream-*"))
        assert not list(root.glob("*.humtrace-tmp-*"))
    # Quota failure cannot publish a partial file or erase an existing report.
    source = root / "tone.wav"
    output = root / "quota.json"
    result = run(source, "--json", output, "--stream", "--memory-limit-mib", 1)
    assert result.returncode == 1 and "memory budget" in result.stderr and not output.exists(), result.stderr
    output.write_text("keep me")
    result = run(source, "--json", output, "--stream", "--memory-limit-mib", 1, "--force")
    assert result.returncode == 1 and output.read_text() == "keep me"
    # Enough rows to hit the 1 MiB spool budget well before completion.
    large = root / "large.wav"
    with wave.open(str(large), "wb") as wav:
        wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(4096)
        block = b"".join(struct.pack("<h", round(10000*math.sin(2*math.pi*60*i/4096))) for i in range(4096))
        for _ in range(300):
            wav.writeframesraw(block)
    result = run(large, "--json", root / "disk.json", "--stream", "--disk-limit-mib", 1)
    assert result.returncode == 1 and "disk quota" in result.stderr, result.stderr
    assert not (root / "disk.json").exists()
    assert not list(root.glob(".humtrace-stream-*"))
    if sys.platform != "win32":
        process = subprocess.Popen([str(exe), str(large), "--stream", "--json", str(root / "cancel.json")],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        time.sleep(0.05)
        process.send_signal(signal.SIGINT)
        _, error = process.communicate(timeout=10)
        assert process.returncode == 130, error
        assert not (root / "cancel.json").exists()
        assert not list(root.glob(".humtrace-stream-*"))
print("Streaming: exact report parity for 5 stereo fixtures; memory/disk quotas, source safety and cleanup passed; cancellation " + ("skipped on Windows" if sys.platform == "win32" else "passed"))
