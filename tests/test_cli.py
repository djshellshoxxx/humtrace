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
    assert report["schema_version"] == "1.1"
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


def validate_schema(report: dict) -> None:
    try:
        import jsonschema
    except ImportError:  # optional dev dependency; CI installs it
        print("jsonschema not installed; skipping schema validation")
        return
    schema_path = pathlib.Path(__file__).resolve().parent.parent / "schema" / "analysis-report-v1.schema.json"
    jsonschema.validate(report, json.loads(schema_path.read_text(encoding="utf-8")))


def check_integrity_tracks_and_notes(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "integrity tones.wav"
    report_path = directory / "integrity.json"
    write_stereo_tones(source)
    before = source.read_bytes()
    result = subprocess.run([str(executable), str(source), "--json", str(report_path)],
                            capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    assert source.read_bytes() == before, "source recording must not be modified"
    assert not (directory / "integrity.json.tmp").exists(), "temporary report file left behind"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    validate_schema(report)
    assert report["input"]["sha256"] == hashlib.sha256(before).hexdigest()
    assert report["input"]["size_bytes"] == len(before)
    assert report["input"]["unchanged_during_analysis"] is True
    assert report["input"]["path"] == "integrity tones.wav", "default path is basename only"
    assert hashlib.sha256(before).hexdigest() in result.stdout
    for channel, frequency in zip(report["channels"], (60.0, 50.0)):
        assert len(channel["tracks"]) == 1, channel["tracks"]
        track = channel["tracks"][0]
        assert abs(track["median_frequency_hz"] - frequency) < 0.3, track
        assert track["observed_frames"] == 3 and track["missing_frames"] == 0
        assert abs(track["drift_hz_per_second"]) < 0.5
    assert report["channels"][0]["tracks"][0]["nearest_note"]["name"] == "B1"
    assert report["channels"][1]["tracks"][0]["nearest_note"]["name"] == "G1"
    assert "B1" in result.stdout

    full = subprocess.run([str(executable), str(source), "--json", str(report_path), "--full-path"],
                          capture_output=True, text=True, check=False)
    assert full.returncode == 0, full.stderr
    assert json.loads(report_path.read_text(encoding="utf-8"))["input"]["path"] == str(source)


def check_comparison(executable: pathlib.Path, directory: pathlib.Path) -> None:
    first = directory / "first.wav"
    second = directory / "second.wav"
    report_path = directory / "comparison.json"
    write_stereo_tones(first)
    sample_rate = 4096
    with wave.open(str(second), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        frames = []
        for frame in range(sample_rate * 2):
            left = round(0.25 * 32767 * math.sin(2 * math.pi * 60.2 * frame / sample_rate))
            right = round(0.25 * 32767 * math.sin(2 * math.pi * 120 * frame / sample_rate))
            frames.append(struct.pack("<hh", left, right))
        output.writeframes(b"".join(frames))
    result = subprocess.run([str(executable), str(first), "--compare", str(second), "--json", str(report_path)],
                            capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    report = json.loads(report_path.read_text(encoding="utf-8"))
    validate_schema(report)
    comparison = report["comparison"]
    assert comparison["alignment"] == "none" and comparison["resampling"] == "none"
    assert comparison["reference"]["sha256"] == hashlib.sha256(second.read_bytes()).hexdigest()
    left, right = comparison["channels"]
    assert len(left["matches"]) == 1 and abs(left["matches"][0]["delta_frequency_hz"] - 0.2) < 0.3
    assert right["matches"] == [] and right["unmatched_track_ids"] and right["unmatched_reference_track_ids"]
    assert "does not show a shared source" in result.stdout


def check_bad_arguments(executable: pathlib.Path, directory: pathlib.Path) -> None:
    for arguments in ([], ["--bogus", "x.wav"], ["a.wav", "--a4", "-1"], ["a.wav", "--json"]):
        result = subprocess.run([str(executable), *arguments], capture_output=True, text=True, check=False)
        assert result.returncode == 2, (arguments, result.returncode)
        assert "Usage" in result.stderr


def check_invalid_file_fails(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "not audio.wav"
    source.write_text("not a wave file", encoding="ascii")
    result = subprocess.run(
        [str(executable), str(source)], capture_output=True, text=True, check=False
    )
    assert result.returncode != 0
    assert "not a RIFF/WAVE file" in result.stderr


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_cli.py <humtrace executable>")
    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="humtrace-cli-") as temporary:
        directory = pathlib.Path(temporary)
        check_stereo_report(executable, directory)
        check_integrity_tracks_and_notes(executable, directory)
        check_comparison(executable, directory)
        check_bad_arguments(executable, directory)
        check_invalid_file_fails(executable, directory)
    print("All CLI integration tests passed.")


if __name__ == "__main__":
    main()
