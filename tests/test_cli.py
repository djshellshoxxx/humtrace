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
    assert report["schema_version"] == "1.2"
    assert report["settings"]["tone_tracking"]["truncated"] is False
    assert all("tracks" in channel for channel in report["channels"])
    assert report["input"]["sha256"] == hashlib.sha256(source.read_bytes()).hexdigest()
    assert report["warnings"] == []
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


def write_mono(path: pathlib.Path, tones, seconds: int, rate: int = 8000) -> None:
    frames = []
    for n in range(rate * seconds):
        t = n / rate
        value = sum(a * math.sin(2 * math.pi * f * t) for f, a, start, end in tones if start <= t < end)
        frames.append(struct.pack("<h", round(max(-1.0, min(1.0, value)) * 32767)))
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(b"".join(frames))


def validate_schema(document: dict, schema_name: str) -> None:
    try:
        import jsonschema  # optional; validation is skipped when not installed
    except ImportError:
        return
    schema_path = pathlib.Path(__file__).resolve().parent.parent / "schema" / schema_name
    jsonschema.validate(document, json.loads(schema_path.read_text(encoding="utf-8")))


def check_tracks_and_comparison(executable: pathlib.Path, directory: pathlib.Path) -> None:
    first = directory / "hum and tone.wav"
    second = directory / "hum only.wav"
    write_mono(first, [(60, 0.2, 0, 8), (180, 0.1, 0, 8), (1000, 0.1, 2, 5)], 8)
    write_mono(second, [(60, 0.2, 0, 6)], 6)
    report_path = directory / "tracks.json"
    result = run(executable, first, "--json", report_path, "--frame-size", "2048", "--no-progress")
    assert result.returncode == 0, result.stderr
    assert "tone tracks: 3" in result.stdout, result.stdout
    report = json.loads(report_path.read_text(encoding="utf-8"))
    validate_schema(report, "analysis-report-v1.schema.json")
    tracks = report["channels"][0]["tracks"]
    by_freq = {round(t["median_frequency_hz"]): t for t in tracks}
    assert set(by_freq) == {60, 180, 1000}, by_freq.keys()
    assert by_freq[60]["near_mains_multiple"] == {"nominal_hz": 60, "harmonic": 1,
                                                 "deviation_hz": by_freq[60]["near_mains_multiple"]["deviation_hz"]}
    assert by_freq[1000]["near_mains_multiple"] is None
    assert 1.4 < by_freq[1000]["start_seconds"] < 2.1 and 4.9 < by_freq[1000]["end_seconds"] < 5.6
    assert by_freq[60]["persistence"] > 0.95

    comparison_path = directory / "comparison.json"
    result = run(executable, "--compare", first, second, "--frame-size", "2048",
                 "--json", comparison_path, "--no-progress")
    assert result.returncode == 0, result.stderr
    comparison = json.loads(comparison_path.read_text(encoding="utf-8"))
    validate_schema(comparison, "comparison-report-v1.schema.json")
    assert comparison["comparison_schema_version"] == "1.0"
    assert [f["sha256"] for f in comparison["files"]] == [
        hashlib.sha256(p.read_bytes()).hexdigest() for p in (first, second)]
    shared = comparison["shared_tones"]
    assert len(shared) == 1 and abs(shared[0]["frequency_hz"] - 60) < 0.5 and shared[0]["files"] == [1, 2]
    assert comparison["interpretation"]["source_classification"] is None
    before = hashlib.sha256(second.read_bytes()).hexdigest()
    result = run(executable, "--compare", first, second, "--json", second, "--no-progress")
    assert result.returncode != 0 and "overwrite" in result.stderr, result
    assert hashlib.sha256(second.read_bytes()).hexdigest() == before


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_cli.py <humtrace executable>")
    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="humtrace-cli-") as temporary:
        directory = pathlib.Path(temporary)
        check_stereo_report(executable, directory)
        check_invalid_file_fails(executable, directory)
        check_safety_and_options(executable, directory)
        check_tracks_and_comparison(executable, directory)
    print("All CLI integration tests passed.")


if __name__ == "__main__":
    main()
