#!/usr/bin/env python3
"""Deterministic synthetic validation of HumTrace measurements (unit-fixture layer only).

Generates seeded WAV fixtures, runs the CLI, and measures frequency error, level error,
detection, and false tracks on tone-free noise. Results validate the implementation against
known signals; they do NOT establish field performance (see docs/specs/validation-corpus.md).

usage: validate_synthetic.py <humtrace executable> [--markdown out.md]
"""
import json
import math
import pathlib
import random
import struct
import subprocess
import sys
import tempfile
import wave

SEED = 20240601
FIXTURE_VERSION = "synthetic-1"


def write_wav(path, rate, samples):
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(b"".join(struct.pack("<h", max(-32768, min(32767, round(s * 32767)))) for s in samples))


def tone(rate, seconds, hz, peak_dbfs, noise_dbfs, rng):
    amp = 10 ** (peak_dbfs / 20)
    sigma = 10 ** (noise_dbfs / 20) if noise_dbfs is not None else 0.0
    w = 2 * math.pi * hz / rate
    return [amp * math.sin(w * i) + (rng.gauss(0, sigma) if sigma else 0.0) for i in range(int(rate * seconds))]


def run(exe, directory, name, rate, samples):
    wav = directory / f"{name}.wav"
    out = directory / f"{name}.json"
    write_wav(wav, rate, samples)
    subprocess.run([str(exe), str(wav), "--json", str(out)], check=True, capture_output=True)
    return json.loads(out.read_text())


def nearest_track(report, hz):
    tracks = report["channels"][0]["tracks"]
    return min(tracks, key=lambda t: abs(t["median_frequency_hz"] - hz), default=None)


def main():
    exe = pathlib.Path(sys.argv[1]).resolve()
    markdown = sys.argv[sys.argv.index("--markdown") + 1] if "--markdown" in sys.argv else None
    rng = random.Random(SEED)
    rows, failures = [], []
    with tempfile.TemporaryDirectory(prefix="humtrace-val-") as tmp:
        d = pathlib.Path(tmp)
        for rate in (8000, 44100):
            for hz in (50.0, 60.0, 100.0, 120.0, 187.3, 1000.0):
                for peak, noise in ((-6.0, None), (-20.0, -60.0), (-40.0, -70.0)):
                    name = f"t{rate}_{hz}_{peak}"
                    report = run(exe, d, name, rate, tone(rate, 6.0, hz, peak, noise, rng))
                    spacing = report["settings"]["tracker"]["bin_spacing_hz"]
                    track = nearest_track(report, hz)
                    detected = track is not None and abs(track["median_frequency_hz"] - hz) <= 1.5 * spacing
                    ferr = abs(track["median_frequency_hz"] - hz) if track else float("nan")
                    lerr = abs(track["median_level_dbfs"] - peak) if track else float("nan")
                    snr = None if noise is None else peak - noise
                    rows.append((rate, hz, peak, snr, spacing, detected, ferr, lerr))
                    if not detected:
                        failures.append(f"missed {hz} Hz at {peak} dBFS (rate {rate}, noise {noise})")
                    elif ferr > 0.25 * spacing or lerr > 0.6:
                        failures.append(f"error too large for {hz} Hz at {peak} dBFS: {ferr:.3f} Hz, {lerr:.2f} dB")
        # Negative set: noise only, silence, and a non-mains tone must not produce mains harmonic support.
        negatives = []
        for label, samples, rate in (
            ("white noise -50 dBFS", [rng.gauss(0, 10 ** (-50 / 20)) for _ in range(8000 * 20)], 8000),
            ("digital silence", [0.0] * (8000 * 5), 8000),
        ):
            report = run(exe, d, label.replace(" ", "_"), rate, samples)
            frames = report["channels"][0]["frames"]
            support = sum(f["mains_50_hz_measurements"]["supporting_harmonics"] + f["mains_60_hz_measurements"]["supporting_harmonics"] for f in frames)
            tracks = report["channels"][0]["tracks"]
            long_tracks = [t for t in tracks if t["observed_frames"] >= 5]
            hours = report["input"]["duration_seconds"] / 3600
            negatives.append((label, len(frames), support, len(tracks), len(long_tracks), len(tracks) / hours))
            if support > 0.01 * len(frames) * 20:
                failures.append(f"{label}: mains harmonic support in noise ({support} of {len(frames) * 20} bins)")
    lines = [f"# Synthetic validation results ({FIXTURE_VERSION}, seed {SEED})", "",
             "Unit-fixture layer only: deterministic generated signals with known truth. These results verify the implementation and",
             "are **not** a claim about performance on real recordings. No pass threshold here is a release gate for field accuracy.", "",
             "## Tone measurement", "",
             "| Rate (Hz) | Tone (Hz) | Peak (dBFS) | SNR (dB) | Bin (Hz) | Detected | Freq error (Hz) | Level error (dB) |", "|---|---|---|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r[0]} | {r[1]} | {r[2]} | {'n/a' if r[3] is None else r[3]} | {r[4]:.3f} | {'yes' if r[5] else 'NO'} | {r[6]:.4f} | {r[7]:.3f} |")
    detected = sum(1 for r in rows if r[5])
    lines += ["", f"Detection: {detected}/{len(rows)} fixtures. Max frequency error: {max(r[6] for r in rows if r[6] == r[6]):.4f} Hz; max level error: {max(r[7] for r in rows if r[7] == r[7]):.3f} dB.", "",
              "## Negative fixtures", "", "| Fixture | Frames | Mains harmonic-support bins | Tracks | Tracks >= 5 frames | Tracks / hour |", "|---|---|---|---|---|---|"]
    for n in negatives:
        lines.append(f"| {n[0]} | {n[1]} | {n[2]} | {n[3]} | {n[4]} | {n[5]:.0f} |")
    lines += ["", "Subgroups not evaluated: real recordings, codec artifacts, music/speech, switching tones, multi-device chains."]
    text = "\n".join(lines) + "\n"
    if markdown:
        pathlib.Path(markdown).write_text(text, encoding="utf-8")
    print(text if not markdown else f"validation: {detected}/{len(rows)} detected, {len(failures)} failure(s)")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
