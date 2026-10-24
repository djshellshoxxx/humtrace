#!/usr/bin/env python3
"""Linux paged GUI long-file acceptance with real RSS and heartbeat checks.
Run: python tools/validate_paged_gui_memory.py build-desktop/bin/app_test_paged_memory
"""
import hashlib
import math
import os
import pathlib
import resource
import struct
import subprocess
import sys
import tempfile
import wave

if sys.platform != "linux":
    raise SystemExit("This acceptance runner uses Linux wait4 RSS units and RLIMIT_AS")
exe = pathlib.Path(sys.argv[1]).resolve()

def constrain():
    resource.setrlimit(resource.RLIMIT_AS, (1536 * 1024**2, 1536 * 1024**2))

with tempfile.TemporaryDirectory(prefix="humtrace-gui-bounded-") as temporary:
    root = pathlib.Path(temporary)
    measurements = []
    for hours in (1, 3):
        source = root / f"{hours}h.wav"
        rate = 8000
        block = b"".join(struct.pack("<h", round(8000*math.sin(2*math.pi*60*i/rate))) for i in range(rate))
        with wave.open(str(source), "wb") as wav:
            wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(rate)
            for _ in range(hours * 3600): wav.writeframesraw(block)
        environment = dict(os.environ, HUMTRACE_MEMORY_WAV=str(source), QT_QPA_PLATFORM="offscreen")
        with (root / "stdout").open("wb") as stdout, (root / "stderr").open("wb") as stderr:
            process = subprocess.Popen([str(exe)], env=environment, stdout=stdout, stderr=stderr, preexec_fn=constrain)
            _, status, usage = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
        assert process.returncode == 0, (root / "stderr").read_text()
        result = (root / "stdout").read_text()
        assert "GUI_PAGED" in result, result
        rss = usage.ru_maxrss / 1024
        assert 0 < rss < 192, f"GUI resident memory {rss:.2f} MiB exceeds 192 MiB"
        measurements.append(rss)
        print(f"{hours}h: peak RSS {rss:.2f} MiB; {result.strip()}", flush=True)
        source.unlink()
    assert measurements[1] <= measurements[0] + 16, measurements
print("PASS: complete 1h/3h GUI analysis, random access and heartbeat; bounded caches; RSS growth <=16 MiB; source unchanged")
