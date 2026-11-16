import hashlib
import json
import math
import pathlib
import struct
import subprocess
import sys
import tempfile
import wave


def write_stereo_tones(path: pathlib.Path) -> None:
    sample_rate = 4096
    frame_count = sample_rate * 2
    samples = []
    for frame in range(frame_count):
        left = round(0.5 * 32767 * math.sin(2 * math.pi * 60 * frame / sample_rate))
        right = round(0.25 * 32767 * math.sin(2 * math.pi * 50 * frame / sample_rate))
        samples.append((left, right))
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(b"".join(struct.pack("<hh", *pair) for pair in samples))


def check_stereo_report(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "two-channel tones.wav"
    report_path = directory / "analysis report.json"
    write_stereo_tones(source)
    result = subprocess.run(
        [str(executable), str(source), "--json", str(report_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["schema_version"] == "1.3"
    assert report["input"]["sha256"] == hashlib.sha256(source.read_bytes()).hexdigest()
    assert report["warnings"] == []
    assert report["settings"]["peak_tracking"]["peaks_per_frame"] == 8
    assert all(track["observed_frames"] >= 3 for channel in report["channels"] for track in channel["tracks"])
    assert report["input"]["sample_rate_hz"] == 4096
    assert report["input"]["channels"] == 2
    assert report["input"]["duration_seconds"] == 2.0
    assert [channel["channel_number"] for channel in report["channels"]] == [1, 2]
    assert [frame["start_sample"] for frame in report["channels"][0]["frames"]] == [0, 2048, 4096]

    expected = ((60.0, -6.02), (50.0, -12.04))
    for channel, (frequency_hz, level_dbfs) in zip(report["channels"], expected):
        peak = channel["frames"][0]["strongest_peak"]
        assert abs(peak["frequency_hz"] - frequency_hz) < 0.2, peak
        assert abs(peak["level_dbfs"] - level_dbfs) < 0.15, peak


    # Default PSD: segment equals the timeline frame (4096), hop is half of it, and every
    # sample of the file is consumed (8192 frames -> 3 complete segments).
    welch = report["settings"]["welch_psd"]
    assert welch["window"] == "hann" and welch["density_unit"] == "dBFS^2/Hz"
    assert welch["segment_size_samples"] == 4096 and welch["hop_size_samples"] == 2048
    for channel in report["channels"]:
        psd = channel["psd"]
        assert psd["samples_processed"] == 8192, psd
        assert psd["segments_averaged"] == 3, psd
        assert abs(psd["bin_width_hz"] - 1.0) < 1e-9, psd
        assert psd["source_classification"] is None
    assert report["channels"][0]["psd"]["mains_60_hz_psd"]["harmonics"][0]["prominent"] is True
    assert report["channels"][1]["psd"]["mains_50_hz_psd"]["harmonics"][0]["prominent"] is True

    # Custom PSD segment and hop: 1 + (8192 - 1024) / 256 = 29 segments.
    custom_path = directory / "custom psd.json"
    result = subprocess.run(
        [str(executable), str(source), "--json", str(custom_path),
         "--psd-segment-size", "1024", "--psd-hop-size", "256"],
        capture_output=True, text=True, check=False,
    )
    assert result.returncode == 0, result.stderr
    assert "Welch PSD (Hann, 1024-sample segments, hop 256, 29 segments" in result.stdout, result.stdout
    custom = json.loads(custom_path.read_text(encoding="utf-8"))
    assert custom["settings"]["welch_psd"]["segment_size_samples"] == 1024
    assert custom["channels"][0]["psd"]["segments_averaged"] == 29


def check_invalid_file_fails(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "not audio.wav"
    source.write_text("not a wave file", encoding="ascii")
    result = subprocess.run(
        [str(executable), str(source)], capture_output=True, text=True, check=False
    )
    assert result.returncode != 0
    assert "not a RIFF/WAVE file" in result.stderr


def run(executable, *args):
    return subprocess.run([str(executable), *map(str, args)], capture_output=True, text=True, check=False)


def check_safety_and_options(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "safe.wav"
    write_stereo_tones(source)
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    result = run(executable, source, "--json", source, "--no-progress")
    assert result.returncode != 0 and "overwrite the input" in result.stderr, result
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
    for bad in (["--frame-size", "1000"], ["--frame-size", "abc"], ["--threshold", "5"], ["--bogus"]):
        result = run(executable, source, *bad, "--no-progress")
        assert result.returncode != 0 and "HumTrace:" in result.stderr, (bad, result)
    for bad in (["--peaks-per-frame", "0"], ["--peaks-per-frame", "65"], ["--track-tolerance-hz", "-1"],
                ["--track-min-frames", "0"], ["--track-max-gap-frames", "x"]):
        result = run(executable, source, *bad, "--no-progress")
        assert result.returncode != 0 and "HumTrace:" in result.stderr, (bad, result)
    tuned = run(executable, source, "--peaks-per-frame", "4", "--track-min-frames", "2", "--no-progress")
    assert tuned.returncode == 0 and "peak tracks" in tuned.stdout and "Track 1:" in tuned.stdout, tuned
    for bad in (["--psd-segment-size", "1000"], ["--psd-segment-size", "2"], ["--psd-hop-size", "0"]):
        result = run(executable, source, *bad, "--no-progress")
        assert result.returncode != 0 and "HumTrace:" in result.stderr, (bad, result)
    # PSD hop larger than the segment is rejected by the engine.
    result = run(executable, source, "--psd-segment-size", "1024", "--psd-hop-size", "2048", "--no-progress")
    assert result.returncode != 0 and "PSD hop" in result.stderr, result
    assert run(executable).returncode == 2
    assert run(executable, "--version").stdout.startswith("humtrace ")
    other = directory / "other.wav"
    write_stereo_tones(other)
    result = run(executable, "--compare", source, other, "--no-progress")
    assert result.returncode == 0 and result.stdout.count("sha256 ") == 2, result
    assert run(executable, "--compare", source, "--no-progress").returncode == 2
    report = directory / "r.json"
    assert run(executable, source, "--json", report, "--no-progress").returncode == 0
    assert run(executable, source, "--json", report, "--no-progress").returncode == 0
    assert (directory / "r.json.bak").exists()


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_cli.py <humtrace executable>")
    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="humtrace-cli-") as temporary:
        directory = pathlib.Path(temporary)
        check_stereo_report(executable, directory)
        check_invalid_file_fails(executable, directory)
        check_safety_and_options(executable, directory)
    print("All CLI integration tests passed.")


if __name__ == "__main__":
    main()
