#!/usr/bin/env python3
"""Linux long-file acceptance: true address-space limit and duration-independent RSS.
Run: python tools/validate_streaming_memory.py build-desktop/bin/humtrace
"""
import hashlib
import math
import os
import pathlib
import resource
import signal
import struct
import subprocess
import sys
import tempfile
import time
import wave

if sys.platform != "linux":
    raise SystemExit("This acceptance runner uses Linux wait4 RSS units and RLIMIT_AS")
exe = pathlib.Path(sys.argv[1]).resolve()

def constrain():
    resource.setrlimit(resource.RLIMIT_AS, (96 * 1024**2, 96 * 1024**2))

with tempfile.TemporaryDirectory(prefix="humtrace-bounded-") as temporary:
    root = pathlib.Path(temporary)
    measurements = []
    for hours in (1, 3):
        source, report = root / f"{hours}h.wav", root / f"{hours}h.json"
        rate = 8000
        block = b"".join(struct.pack("<h", round(8000*math.sin(2*math.pi*60*i/rate))) for i in range(rate))
        with wave.open(str(source), "wb") as wav:
            wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(rate)
            for _ in range(hours * 3600):
                wav.writeframesraw(block)
        with source.open("rb") as stream:
            before = hashlib.file_digest(stream, "sha256").hexdigest()
        with (root / "stdout").open("wb") as stdout, (root / "stderr").open("wb") as stderr:
            process = subprocess.Popen([str(exe), str(source), "--stream", "--json", str(report),
                                        "--memory-limit-mib", "64", "--disk-limit-mib", "1024"],
                                       stdout=stdout, stderr=stderr, preexec_fn=constrain)
            _, status, usage = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
        assert process.returncode == 0, (root / "stderr").read_text()
        rss = usage.ru_maxrss / 1024
        assert 0 < rss < 64, f"resident memory {rss:.2f} MiB exceeds 64 MiB"
        counts = 0
        carry = b""
        needle = b'"sample_levels":'
        with report.open("rb") as stream:
            while chunk := stream.read(65536):
                combined = carry + chunk
                counts += combined.count(needle)
                carry = combined[-(len(needle) - 1):]
        total_samples = hours * 3600 * rate
        expected = (total_samples - 4096) // 2048 + 1 + bool((total_samples - 4096) % 2048)
        assert counts == expected, (counts, expected)
        with source.open("rb") as stream:
            assert hashlib.file_digest(stream, "sha256").hexdigest() == before
        assert not list(root.glob(".humtrace-stream-*"))
        assert not list(root.glob("*.humtrace-tmp-*"))
        measurements.append(rss)
        print(f"{hours}h: {counts} complete measurements, {report.stat().st_size} report bytes, peak RSS {rss:.2f} MiB", flush=True)
        if hours == 3:
            cancelled_report = root / "cancelled.json"
            with (root / "cancel-stdout").open("wb") as stdout, (root / "cancel-stderr").open("wb") as stderr:
                process = subprocess.Popen([str(exe), str(source), "--stream", "--json", str(cancelled_report),
                                            "--memory-limit-mib", "64", "--disk-limit-mib", "1024"],
                                           stdout=stdout, stderr=stderr, preexec_fn=constrain)
                time.sleep(0.1)
                if process.poll() is None:
                    started = time.monotonic()
                    process.send_signal(signal.SIGINT)
                    _, status, _ = os.wait4(process.pid, 0)
                    process.returncode = os.waitstatus_to_exitcode(status)
                    latency = time.monotonic() - started
                else:
                    latency = float("inf")
            error_text = (root / "cancel-stderr").read_text(errors="replace")
            assert process.returncode == 130 and latency < 1.0, (process.returncode, latency, error_text)
            assert "cancelled" in error_text.lower() and not cancelled_report.exists(), error_text
            assert not list(root.glob(".humtrace-stream-*"))
            assert not list(root.glob("*.humtrace-tmp-*"))
            print(f"3h bounded-memory cancellation: exit 130, {latency:.3f}s, no report or scratch files", flush=True)
        source.unlink(); report.unlink()
    assert measurements[1] <= measurements[0] + 8, measurements
print("PASS: 1h/3h streaming reports complete under 96 MiB address-space limit; RSS growth <=8 MiB; source unchanged and scratch cleaned")
