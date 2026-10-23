#!/usr/bin/env python3
"""Run HumTrace on real-world WAV files and cross-check it against an independent estimator.

For every file: decode with soundfile, run the CLI, then for sampled frames recompute the
Hann-windowed spectrum with numpy (mean removed, same gain convention) and compare the
strongest non-DC peak frequency and level with HumTrace's. Also records mains-harmonic
evidence and track counts. Real recordings here have NO ground-truth labels: this validates
decoder/estimator agreement and false-positive behavior, not detection accuracy.

usage: real_recordings_check.py <humtrace exe> <dir with wav files> [--markdown out.md] [--manifest m.json]
"""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

def numpy_peak(window, rate):
    n = len(window)
    w = np.hanning(n)  # 0.5 - 0.5 cos(2 pi i / (n - 1))
    spectrum = np.fft.rfft((window - window.mean()) * w)
    amp = np.abs(spectrum) / w.sum()
    amp[1:-1] *= 2.0
    k = 1 + int(np.argmax(amp[1:]))
    db = 20 * np.log10(np.maximum(amp, 1e-300))
    if 0 < k < len(db) - 1 and np.isfinite(db[k - 1]) and np.isfinite(db[k + 1]):
        left, center, right = db[k - 1], db[k], db[k + 1]
        curvature = left - 2 * center + right
        if curvature < 0:
            offset = float(np.clip(0.5 * (left - right) / curvature, -0.5, 0.5))
            return (k + offset) * rate / n, center - 0.25 * (left - right) * offset
    return k * rate / n, db[k]


def main():
    global np, sf
    exe, folder = pathlib.Path(sys.argv[1]).resolve(), pathlib.Path(sys.argv[2])
    if not folder.is_dir():
        raise SystemExit(f"real-recordings directory does not exist: {folder}")
    audio_paths = sorted(folder.glob("*.wav"))
    if not audio_paths:
        raise SystemExit(f"no real WAV recordings found in: {folder}")
    try:
        import numpy as np
        import soundfile as sf
    except ImportError as exc:
        raise SystemExit(f"real-recording checks require numpy and soundfile: {exc}") from exc
    out_md = sys.argv[sys.argv.index("--markdown") + 1] if "--markdown" in sys.argv else None
    out_manifest = sys.argv[sys.argv.index("--manifest") + 1] if "--manifest" in sys.argv else None
    rows, manifest, problems = [], [], []
    with tempfile.TemporaryDirectory() as tmp:
        for path in audio_paths:
            report_path = pathlib.Path(tmp) / (path.stem + ".json")
            proc = subprocess.run([str(exe), str(path), "--json", str(report_path)], capture_output=True, text=True)
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            manifest.append({"file": path.name, "bytes": path.stat().st_size, "sha256": digest})
            if proc.returncode != 0:
                rows.append((path.name, "-", "-", "-", f"rejected: {proc.stderr.strip()[:60]}", "-", "-", "-", "-"))
                if "unsupported" not in proc.stderr and "too short" not in proc.stderr:
                    problems.append(f"{path.name}: unexpected failure: {proc.stderr.strip()}")
                continue
            report = json.loads(report_path.read_text())
            if report["input"]["sha256"] != digest:
                problems.append(f"{path.name}: sha256 mismatch")
            data, rate = sf.read(path, dtype="float64", always_2d=True)
            size = report["settings"]["frame_size_samples"]
            max_df = max_dl = 0.0
            compared = 0
            mains_frames = 0
            total_frames = 0
            for ch_index, channel in enumerate(report["channels"]):
                frames = channel["frames"]
                total_frames += len(frames)
                step = max(1, len(frames) // 60)
                for frame in frames[::step]:
                    start = frame["start_sample"]
                    f_np, l_np = numpy_peak(data[start:start + size, ch_index], rate)
                    peak = frame["strongest_peak"]
                    if peak["frequency_hz"] is None or peak["level_dbfs"] is None:
                        continue
                    max_df = max(max_df, abs(peak["frequency_hz"] - f_np))
                    max_dl = max(max_dl, abs(peak["level_dbfs"] - l_np))
                    compared += 1
                for frame in frames:
                    for key in ("mains_50_hz_measurements", "mains_60_hz_measurements"):
                        if frame[key]["supporting_harmonics"] >= 3:
                            mains_frames += 1
                            break
            tracks = sum(len(c["tracks"]) for c in report["channels"])
            rows.append((path.name, rate, data.shape[1], f"{len(data) / rate:.1f}", "ok", compared,
                         f"{max_df:.4f}", f"{max_dl:.3f}", f"{mains_frames}/{total_frames}", tracks))
            if compared and (max_df > 0.01 or max_dl > 0.05):
                problems.append(f"{path.name}: disagrees with numpy (df={max_df:.4f} Hz, dl={max_dl:.3f} dB)")
    lines = ["# Real-recording cross-check", "",
             "Independent estimator (numpy, same Hann/mean-removal/parabolic conventions) vs HumTrace on real files. "
             "The files have no mains-hum ground truth, so this checks decoding, estimator agreement and false-positive behavior only.", "",
             "| File | Rate | Ch | Sec | Status | Frames compared | Max freq diff (Hz) | Max level diff (dB) | Frames with >=3 mains bins | Tracks |",
             "|---|---|---|---|---|---|---|---|---|---|"]
    lines += ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]
    lines += ["", f"Problems: {len(problems)}" + ("".join(f"\n- {p}" for p in problems))]
    text = "\n".join(lines) + "\n"
    if out_md:
        pathlib.Path(out_md).write_text(text)
    if out_manifest:
        pathlib.Path(out_manifest).write_text(json.dumps(manifest, indent=2) + "\n")
    print(text if not out_md else f"{len(rows)} files, {len(problems)} problem(s)")
    for p in problems:
        print("PROBLEM", p, file=sys.stderr)
    sys.exit(1 if problems else 0)


main()
