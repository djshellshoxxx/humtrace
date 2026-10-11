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
    assert report["schema_version"] == "1.0"
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


def write_extensible_tone(path: pathlib.Path) -> None:
    sample_rate = 4096
    frames = b"".join(
        struct.pack("<i", round(0.5 * 2147483647 * math.sin(2 * math.pi * 60 * n / sample_rate)) & ~0xFFF)[1:]
        for n in range(sample_rate)
    )
    subformat_pcm = struct.pack("<IHH", 1, 0, 0x10) + bytes.fromhex("800000aa00389b71")
    fmt = struct.pack("<HHIIHHHHI", 0xFFFE, 1, sample_rate, sample_rate * 3, 3, 24, 22, 20, 0x4) + subformat_pcm
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(frames)) + frames
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


def check_extensible_input(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "extensible.wav"
    report_path = directory / "extensible.json"
    write_extensible_tone(source)
    result = subprocess.run(
        [str(executable), str(source), "--json", str(report_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    assert "Valid bits: 20" in result.stdout, result.stdout
    assert "Channel mask: 0x4" in result.stdout, result.stdout
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["input"]["format_code"] == 1
    assert report["input"]["bit_depth"] == 24
    peak = report["channels"][0]["frames"][0]["strongest_peak"]
    assert abs(peak["frequency_hz"] - 60.0) < 0.2, peak
    assert abs(peak["level_dbfs"] + 6.02) < 0.15, peak


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
        check_extensible_input(executable, directory)
        check_invalid_file_fails(executable, directory)
    print("All CLI integration tests passed.")


if __name__ == "__main__":
    main()
