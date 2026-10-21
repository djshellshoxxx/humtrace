import json
import math
import os
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
    assert report["input"]["sample_rate_hz"] == 4096
    assert report["input"]["channels"] == 2
    assert report["input"]["duration_seconds"] == 2.0
    import hashlib
    try:
        import jsonschema
    except ImportError:
        if os.environ.get("HUMTRACE_REQUIRE_JSONSCHEMA"):
            raise  # CI sets this so the schema check can never be skipped silently
        jsonschema = None
    if jsonschema:
        schema_path = pathlib.Path(__file__).resolve().parent.parent / "schema" / "analysis-report-v1.schema.json"
        jsonschema.validate(report, json.loads(schema_path.read_text(encoding="utf-8")))
    assert report["input"]["sha256"] == hashlib.sha256(source.read_bytes()).hexdigest()
    for channel in report["channels"]:
        assert len(channel["tracks"]) == 1 and channel["tracks"][0]["observed_frames"] == 3, channel["tracks"]
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


def check_compare(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "two-channel tones.wav"
    output = directory / "comparison.json"
    result = subprocess.run(
        [str(executable), "compare", str(source), str(source), "--json", str(output)],
        capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    comparison = json.loads(output.read_text(encoding="utf-8"))
    assert comparison["alignment"]["applied"] is False and comparison["resampling"]["applied"] is False
    assert len(comparison["matches"]) == 2 and not comparison["unmatched_a"] and not comparison["unmatched_b"]
    assert all(m["delta_frequency_hz"] == 0 for m in comparison["matches"])
    assert comparison["inputs"][0]["sha256"] == comparison["inputs"][1]["sha256"]


def check_never_overwrites_input(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "protect me.wav"
    source.write_bytes((directory / "two-channel tones.wav").read_bytes())
    before = source.read_bytes()
    result = subprocess.run([str(executable), str(source), "--json", str(source)], capture_output=True, text=True, check=False)
    assert result.returncode == 1 and "overwrite" in result.stderr, result.stderr
    assert source.read_bytes() == before
    result = subprocess.run([str(executable), "compare", str(source), str(source), "--json", str(source)], capture_output=True, text=True, check=False)
    assert result.returncode == 1 and source.read_bytes() == before


def check_paths_overwrite_and_clipping(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "two-channel tones.wav"
    out = directory / "redacted.json"
    result = subprocess.run([str(executable), str(source), "--json", str(out), "--quiet"], capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    report = json.loads(out.read_text(encoding="utf-8"))
    assert report["input"]["path"] == "two-channel tones.wav", report["input"]["path"]  # basename by default
    assert report["input"]["bytes"] == source.stat().st_size
    assert report["generator"]["application_version"] and "pipeline" in report["settings"]
    assert "Channel 1 measured frames" in result.stdout and "s: strongest" not in result.stdout  # --quiet
    # Existing output is refused unless --force.
    again = subprocess.run([str(executable), str(source), "--json", str(out)], capture_output=True, text=True, check=False)
    assert again.returncode == 1 and "--force" in again.stderr and json.loads(out.read_text(encoding="utf-8")) == report
    forced = subprocess.run([str(executable), str(source), "--json", str(out), "--force", "--full-path"], capture_output=True, text=True, check=False)
    assert forced.returncode == 0 and json.loads(out.read_text(encoding="utf-8"))["input"]["path"].endswith("two-channel tones.wav")
    assert json.loads(out.read_text(encoding="utf-8"))["input"]["path"] != "two-channel tones.wav"
    # A heavily clipped recording yields a warning and a clipped-sample count.
    clipped = directory / "clipped.wav"
    with wave.open(str(clipped), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(4096)
        w.writeframes(b"".join(struct.pack("<h", 32767 if math.sin(2 * math.pi * 60 * i / 4096) >= 0 else -32768) for i in range(16384)))
    cout = directory / "clipped.json"
    r = subprocess.run([str(executable), str(clipped), "--json", str(cout)], capture_output=True, text=True, check=False)
    rep = json.loads(cout.read_text(encoding="utf-8"))
    assert r.returncode == 0 and rep["channels"][0]["clipped_samples"] > 10000 and any("full scale" in w for w in rep["warnings"]), rep["warnings"]
    assert "Warning:" in r.stdout
    # Explicit channel map is recorded in the comparison.
    cmp_out = directory / "mapped.json"
    r = subprocess.run([str(executable), "compare", str(source), str(source), "--map", "1:2,2:1", "--json", str(cmp_out)], capture_output=True, text=True, check=False)
    assert r.returncode == 0, r.stderr
    assert json.loads(cmp_out.read_text(encoding="utf-8"))["settings"]["channel_map"] == [[1, 2], [2, 1]]
    bad = subprocess.run([str(executable), "compare", str(source), str(source), "--map", "1:1,1:2"], capture_output=True, text=True, check=False)
    assert bad.returncode == 1 and "at most once" in bad.stderr


def check_early_option_validation(executable: pathlib.Path, directory: pathlib.Path) -> None:
    import time
    source = directory / "two-channel tones.wav"
    for extra in (["--tolerance", "-1"], ["--map", "1:5"], ["--map", "1:1,1:2"], ["--tolerance", "1abc"]):
        started = time.monotonic()
        r = subprocess.run([str(executable), "compare", str(source), str(source), *extra], capture_output=True, text=True, check=False)
        assert r.returncode == 1 and "HumTrace:" in r.stderr, (extra, r.stderr)
        assert time.monotonic() - started < 1.0, extra


def check_unwritable_output_leaves_nothing(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "two-channel tones.wav"
    target = directory / "no-such-dir" / "report.json"
    result = subprocess.run([str(executable), str(source), "--json", str(target)],
                            capture_output=True, text=True, check=False)
    assert result.returncode == 1 and "output directory does not exist" in result.stderr, result.stderr
    assert not target.exists() and not list(directory.rglob("*.humtrace-tmp*"))


def check_interrupt_cancels_without_output(executable: pathlib.Path, directory: pathlib.Path) -> None:
    if sys.platform == "win32":
        return
    import signal
    import time
    long_wav = directory / "long.wav"
    rate = 8000
    with wave.open(str(long_wav), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        chunk = struct.pack("<h", 1000) * rate
        for _ in range(3600):  # one hour of audio
            out.writeframes(chunk)
    report = directory / "cancelled.json"
    process = subprocess.Popen([str(executable), str(long_wav), "--json", str(report)],
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    time.sleep(0.5)
    started = time.monotonic()
    process.send_signal(signal.SIGINT)
    _, stderr = process.communicate(timeout=30)
    assert process.returncode == 130, (process.returncode, stderr)
    assert time.monotonic() - started < 5, "cancellation latency"
    assert "cancelled" in stderr and not report.exists()


def check_csv_and_notes(executable: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "notes tones.wav"
    write_stereo_tones(source)
    csv_path = directory / "tracks.csv"
    result = subprocess.run([str(executable), str(source), "--csv", str(csv_path), "--a4", "440"],
                            capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    assert "(B1 " in result.stdout, result.stdout  # 60 Hz track labelled with its nearest note
    rows = csv_path.read_text(encoding="utf-8").splitlines()
    assert rows[0].startswith("channel,track_id,start_seconds"), rows[0]
    assert any(r.startswith("1,") and ",B1," in r for r in rows[1:]), rows
    assert any(r.startswith("2,") and ",G1," in r for r in rows[1:]), rows
    again = subprocess.run([str(executable), str(source), "--csv", str(csv_path)],
                           capture_output=True, text=True, check=False)
    assert again.returncode != 0, "CSV must not silently overwrite an existing file"
    bad = subprocess.run([str(executable), str(source), "--a4", "10"], capture_output=True, text=True, check=False)
    assert bad.returncode != 0 and "--a4" in bad.stderr, bad.stderr


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_cli.py <humtrace executable>")
    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="humtrace-cli-") as temporary:
        directory = pathlib.Path(temporary)
        check_stereo_report(executable, directory)
        check_invalid_file_fails(executable, directory)
        check_compare(executable, directory)
        check_never_overwrites_input(executable, directory)
        check_early_option_validation(executable, directory)
        check_paths_overwrite_and_clipping(executable, directory)
        check_unwritable_output_leaves_nothing(executable, directory)
        check_interrupt_cancels_without_output(executable, directory)
        check_csv_and_notes(executable, directory)
    print("All CLI integration tests passed.")


if __name__ == "__main__":
    main()
