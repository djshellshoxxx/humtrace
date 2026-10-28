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
