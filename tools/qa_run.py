#!/usr/bin/env python3
"""Repeatable QA matrix for the HumTrace CLI (format coverage, corruption handling, source
immutability, schema validity, determinism, cancellation latency, memory scaling).

usage: qa_run.py <humtrace executable> [--markdown out.md]
"""
import hashlib
import json
import math
import os
import pathlib
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import time

EXE = pathlib.Path(sys.argv[1]).resolve()
SCHEMA = json.loads((pathlib.Path(__file__).resolve().parent.parent / "schema" / "analysis-report-v1.schema.json").read_text(encoding="utf-8"))
try:
    import jsonschema
except ImportError:
    jsonschema = None
results = []


def record(name, ok, detail=""):
    results.append((name, ok, detail))
    print(("PASS " if ok else "FAIL ") + name + (f" - {detail}" if detail else ""))


def wav_bytes(rate, channels, fmt, seconds, tones, extensible=False):
    bits = {"pcm8": 8, "pcm16": 16, "pcm24": 24, "pcm32": 32, "f32": 32, "f64": 64}[fmt]
    code = 3 if fmt.startswith("f") else 1
    frames = int(rate * seconds)
    data = bytearray()
    for i in range(frames):
        for c in range(channels):
            v = sum(0.4 / len(tones) * math.sin(2 * math.pi * f * (c + 1) * i / rate) for f in tones)
            if fmt == "pcm8":
                data += struct.pack("<B", round(v * 127) + 128)
            elif fmt == "pcm16":
                data += struct.pack("<h", round(v * 32767))
            elif fmt == "pcm24":
                data += struct.pack("<i", round(v * 8388607))[:3]
            elif fmt == "pcm32":
                data += struct.pack("<i", round(v * 2147483647))
            elif fmt == "f32":
                data += struct.pack("<f", v)
            else:
                data += struct.pack("<d", v)
    block = channels * bits // 8
    if extensible:
        fmt_chunk = (struct.pack("<HHIIHH", 0xFFFE, channels, rate, rate * block, block, bits)
                     + struct.pack("<HHI", 22, bits, 0) + struct.pack("<H", code)
                     + bytes([0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xAA, 0, 0x38, 0x9B, 0x71]))
    else:
        fmt_chunk = struct.pack("<HHIIHH", code, channels, rate, rate * block, block, bits)
    body = b"fmt " + struct.pack("<I", len(fmt_chunk)) + fmt_chunk + b"data" + struct.pack("<I", len(data)) + bytes(data)
    if len(data) & 1:
        body += b"\0"
    return b"RIFF" + struct.pack("<I", len(body) + 4) + b"WAVE" + body


def run(args, **kw):
    return subprocess.run([str(EXE), *map(str, args)], capture_output=True, text=True, encoding="utf-8", **kw)


def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def max_rss_mb(args):
    """Return child peak RSS and exit status (wait4 reports per-child RSS in KB on Linux)."""
    proc = subprocess.Popen([str(EXE), *map(str, args)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    _, status, usage = os.wait4(proc.pid, 0)
    proc.returncode = os.waitstatus_to_exitcode(status)
    return usage.ru_maxrss / 1024, proc.returncode


def main():
    with tempfile.TemporaryDirectory(prefix="humtrace-qa-") as tmp:
        d = pathlib.Path(tmp)
        # 1. Format matrix: every advertised encoding, channel count, plain/extensible header, two rates.
        for fmt in ("pcm8", "pcm16", "pcm24", "pcm32", "f32", "f64"):
            for channels in (1, 2, 3):
                for ext in (False, True):
                    for rate in (8000, 48000):
                        name = f"{fmt}-{channels}ch-{rate}-{'ext' if ext else 'plain'}"
                        path = d / f"{name}.wav"
                        path.write_bytes(wav_bytes(rate, channels, fmt, 2.0, [60.0], ext))
                        before = sha(path)
                        out = d / f"{name}.json"
                        r = run([path, "--json", out])
                        ok = r.returncode == 0 and sha(path) == before
                        detail = r.stderr.strip()
                        if ok:
                            report = json.loads(out.read_text(encoding="utf-8"))
                            # Pick the track nearest the injected tone (quantization noise can add extra tracks).
                            freq = [min((t["median_frequency_hz"] for t in ch["tracks"]), key=lambda f: abs(f - 60.0 * (c + 1))) / (c + 1)
                                    for c, ch in enumerate(report["channels"]) if ch["tracks"]]
                            tol = 0.6 if rate == 8000 else 0.2
                            ok = (report["input"]["sha256"] == before and len(report["channels"]) == channels
                                  and len(freq) == channels and all(abs(f - 60.0) < tol * (c + 1) for c, f in enumerate(freq)))
                            if jsonschema:
                                jsonschema.validate(report, SCHEMA)
                            detail = f"freq/channel-scale {[round(f, 3) for f in freq]}"
                        record(f"format {name}", ok, detail)
        # 2. Corrupt / hostile files must fail cleanly: exit 1, message, no output, source intact.
        good = wav_bytes(8000, 1, "pcm16", 2.0, [60.0])
        cases = {
            "empty": b"", "garbage": b"\x00" * 100, "truncated-header": good[:20], "truncated-data": good[: len(good) // 2],
            "bad-riff-size": good[:4] + struct.pack("<I", 0xFFFFFFF0) + good[8:],
            "zero-channels": good[:22] + b"\x00\x00" + good[24:], "zero-rate": good[:24] + b"\x00\x00\x00\x00" + good[28:],
            "zero-block-align": good[:32] + b"\x00\x00" + good[34:],
            "wrong-block-align": good[:32] + b"\x04\x00" + good[34:],
            "unsupported-bit-depth": good[:34] + b"\x0c\x00" + good[36:],
            "unsupported-codec": good[:20] + struct.pack("<H", 0x55) + good[22:],
            "too-short": wav_bytes(8000, 1, "pcm16", 15 / 8000, [60.0]),
        }
        # A correctly-sized float WAV with a NaN sample exercises sample validation after parsing.
        nan_sample = struct.pack("<f", float("nan")) + b"\0" * (4 * 4095)
        nan_fmt = struct.pack("<HHIIHH", 3, 1, 8000, 32000, 4, 32)
        nan_body = (b"fmt " + struct.pack("<I", 16) + nan_fmt + b"data"
                    + struct.pack("<I", len(nan_sample)) + nan_sample)
        cases["non-finite-float"] = b"RIFF" + struct.pack("<I", len(nan_body) + 4) + b"WAVE" + nan_body
        partial_body = good[12:36] + b"data" + struct.pack("<I", 3) + good[44:47] + b"\0"
        cases["incomplete-sample-frame"] = b"RIFF" + struct.pack("<I", len(partial_body) + 4) + b"WAVE" + partial_body
        expected_errors = {
            "zero-block-align": "invalid channel, rate, or alignment",
            "wrong-block-align": "inconsistent WAVE sample format",
            "unsupported-bit-depth": "inconsistent WAVE sample format",
            "non-finite-float": "non-finite floating-point sample",
            "incomplete-sample-frame": "incomplete sample frame",
        }
        for name, content in cases.items():
            path = d / f"bad-{name}.wav"
            path.write_bytes(content)
            out = d / f"bad-{name}.json"
            r = run([path, "--json", out], timeout=30)
            expected_error = expected_errors.get(name)
            ok = (r.returncode == 1 and "HumTrace:" in r.stderr and not out.exists()
                  and path.read_bytes() == content
                  and (expected_error is None or expected_error in r.stderr))
            record(f"corrupt {name}", ok, r.stderr.strip()[:100])
        record("missing file", run([d / "nope.wav"]).returncode == 1)
        record("usage error", run([]).returncode == 2)
        # 3. Determinism.
        path = d / "pcm16-2ch-8000-plain.wav"
        outs = []
        for i in range(3):
            o = d / f"det{i}.json"
            run([path, "--json", o])
            outs.append(o.read_bytes())
        record("deterministic reports", outs[0] == outs[1] == outs[2])
        # 4. Output safety.
        r = run([path, "--json", d / "missing" / "x.json"])
        record("unwritable destination", r.returncode == 1 and not list(d.glob("missing*")))
        existing = d / "existing.json"
        existing.write_text("OLD")
        refused = run([path, "--json", existing])
        record("existing output refused without --force", refused.returncode == 1 and existing.read_text(encoding="utf-8") == "OLD")
        run([path, "--json", existing, "--force"])
        record("--force replaces existing, no temp left",
               existing.read_text(encoding="utf-8").startswith("{") and not list(d.glob("existing.json.humtrace-tmp*")))
        unicode_source = d / "音声-über.wav"
        unicode_source.write_bytes(path.read_bytes())
        unicode_out = d / "résultat-解析.json"
        unicode_run = run([unicode_source, "--json", unicode_out, "--full-path"])
        unicode_ok = unicode_run.returncode == 0 and not list(d.glob("résultat-解析.json.humtrace-tmp*"))
        if unicode_ok:
            unicode_report = json.loads(unicode_out.read_text(encoding="utf-8"))
            unicode_ok = pathlib.Path(unicode_report["input"]["path"]).name == unicode_source.name
            first_bytes = unicode_out.read_bytes()
            second_out = d / "répété.json"
            repeated = run([unicode_source, "--json", second_out, "--full-path"])
            unicode_ok = unicode_ok and repeated.returncode == 0 and second_out.read_bytes() == first_bytes
        record("UTF-8 paths round-trip and export deterministically", unicode_ok, unicode_run.stderr.strip())
        # 5. Comparison semantics.
        a, b = d / "a.wav", d / "b.wav"
        a.write_bytes(wav_bytes(8000, 1, "pcm16", 4.0, [50.0, 150.0]))
        b.write_bytes(wav_bytes(8000, 1, "pcm16", 4.0, [50.4, 300.0]))
        c = d / "cmp.json"
        r = run(["compare", a, b, "--json", c])
        cmp = json.loads(c.read_text(encoding="utf-8")) if c.exists() else {}
        record("compare: near tones matched, others unmatched, no alignment",
               r.returncode == 0 and len(cmp.get("matches", [])) == 1 and len(cmp["unmatched_a"]) == 1
               and len(cmp["unmatched_b"]) == 1 and cmp["alignment"]["applied"] is False)
        record("compare rejects negative tolerance", run(["compare", a, b, "--tolerance", "-1"]).returncode == 1)

        # 6. Cancellation latency and memory scaling on long recordings.
        def long_wav(hours):
            p = d / f"long{hours}.wav"
            rate = 8000
            seconds = int(3600 * hours)
            data_size = seconds * rate * 2
            chunk = b"".join(struct.pack("<h", round(8000 * math.sin(2 * math.pi * 60 * i / rate))) for i in range(rate))
            with open(p, "wb") as f:
                f.write(b"RIFF" + struct.pack("<I", 36 + data_size) + b"WAVEfmt "
                        + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" + struct.pack("<I", data_size))
                for _ in range(seconds):
                    f.write(chunk)
            return p
        p1, p3 = long_wav(1), long_wav(3)
        m1, exit1 = max_rss_mb([p1])
        m3, exit3 = max_rss_mb([p3])
        memory_ok = exit1 == 0 and exit3 == 0 and m3 < m1 * 2.5
        record("peak memory growth, 3 h vs 1 h recording", memory_ok,
               f"1 h {m1:.0f} MB (exit {exit1}), 3 h {m3:.0f} MB (exit {exit3})")
        proc = subprocess.Popen([str(EXE), str(p3), "--json", str(d / "c.json")], stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE, text=True, encoding="utf-8")
        time.sleep(0.3)
        t1 = time.time()
        proc.send_signal(signal.SIGINT)
        proc.communicate(timeout=30)
        latency = time.time() - t1
        record("cancel latency < 1 s on 3 h file, no output", proc.returncode == 130 and latency < 1.0
               and not (d / "c.json").exists(), f"{latency:.2f} s")
        shutil.rmtree(d, ignore_errors=True)
    failed = [r for r in results if not r[1]]
    print(f"\nQA: {len(results) - len(failed)}/{len(results)} passed")
    if "--markdown" in sys.argv:
        lines = ["# CLI QA matrix results", "", f"{len(results) - len(failed)}/{len(results)} checks passed.", "",
                 "| Check | Result | Detail |", "|---|---|---|"]
        lines += [f"| {n} | {'pass' if ok else '**FAIL**'} | {det} |" for n, ok, det in results]
        pathlib.Path(sys.argv[sys.argv.index("--markdown") + 1]).write_text("\n".join(lines) + "\n")
    sys.exit(1 if failed else 0)


main()
