import hashlib
import json
import math
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import wave


ROOT = pathlib.Path(__file__).resolve().parent.parent
VERIFIER = ROOT / "tools" / "verify_report.py"
MAX_REPORT_BYTES = 64 * 1024 * 1024


def run_verifier(report: pathlib.Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(VERIFIER), str(report), *arguments],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )


def write_source(path: pathlib.Path) -> None:
    rate = 4096
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(b"".join(
            struct.pack("<h", round(9000 * math.sin(2 * math.pi * 60 * index / rate)))
            for index in range(rate * 2)
        ))


def check_fail(report: pathlib.Path, message: str, *arguments: str) -> None:
    result = run_verifier(report, *arguments)
    assert result.returncode == 2, (result.returncode, result.stdout, result.stderr)
    assert message in result.stderr, result.stderr


def main() -> int:
    try:
        import jsonschema  # noqa: F401
    except ImportError:
        if os.environ.get("HUMTRACE_REQUIRE_JSONSCHEMA"):
            raise
        print("Report verifier tests skipped: Python package 'jsonschema' is not installed")
        return 0

    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="humtrace-report-verify-") as temporary:
        directory = pathlib.Path(temporary)
        source = directory / "source.wav"
        report = directory / "analysis.json"
        write_source(source)
        generated = subprocess.run(
            [str(executable), str(source), "--json", str(report)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=False,
        )
        assert generated.returncode == 0, generated.stderr

        valid = run_verifier(report, "--input", str(source))
        assert valid.returncode == 0, valid.stderr
        expected_input_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        assert f"Input bytes match report: {expected_input_hash}" in valid.stdout
        assert "Schema valid: HumTrace analysis report 1.3" in valid.stdout
        assert "do not authenticate origin" in valid.stdout

        sidecar = directory / "analysis.sha256"
        written = run_verifier(report, "--write-digest", str(sidecar))
        assert written.returncode == 0, written.stderr
        report_hash = hashlib.sha256(report.read_bytes()).hexdigest()
        assert sidecar.read_text(encoding="ascii") == report_hash + "\n"
        sidecar_check = run_verifier(report, "--digest-file", str(sidecar))
        assert sidecar_check.returncode == 0, sidecar_check.stderr
        assert "matches sidecar" in sidecar_check.stdout

        refused = run_verifier(report, "--write-digest", str(sidecar))
        assert refused.returncode == 2 and "refusing to replace" in refused.stderr
        assert sidecar.read_text(encoding="ascii") == report_hash + "\n"
        replaced = run_verifier(report, "--write-digest", str(sidecar), "--force")
        assert replaced.returncode == 0, replaced.stderr
        assert sidecar.read_text(encoding="ascii") == report_hash + "\n"

        changed_source = directory / "changed.wav"
        changed_bytes = bytearray(source.read_bytes())
        changed_bytes[-1] ^= 1
        changed_source.write_bytes(changed_bytes)
        check_fail(report, "SHA-256 does not match", "--input", str(changed_source))

        report_bytes = report.read_bytes()
        sidecar.write_text("0" * 64 + "\n", encoding="ascii")
        check_fail(report, "does not match", "--digest-file", str(sidecar))

        malformed = directory / "malformed.json"
        duplicate = report_bytes.replace(
            b'"application": "HumTrace"', b'"application": "HumTrace", "application": "HumTrace"', 1
        )
        malformed.write_bytes(duplicate)
        check_fail(malformed, "duplicate object key")

        nonfinite = report_bytes.replace(b'"duration_seconds": 2', b'"duration_seconds": NaN', 1)
        malformed.write_bytes(nonfinite)
        check_fail(malformed, "non-finite number")

        overflow_float = report_bytes.replace(b'"duration_seconds": 2', b'"duration_seconds": 1e999', 1)
        malformed.write_bytes(overflow_float)
        check_fail(malformed, "outside the finite numeric range")

        malformed.write_bytes(report_bytes + b"\xff")
        check_fail(malformed, "invalid report JSON")

        malformed.write_bytes(b"[" * 2048 + b"]" * 2048)
        check_fail(malformed, "invalid report JSON")

        future_report = json.loads(report_bytes)
        future_report["schema_version"] = "2.0"
        malformed.write_text(json.dumps(future_report), encoding="utf-8")
        check_fail(malformed, "unsupported report schema version")

        wrong_schema = json.loads(report_bytes)
        wrong_schema["application"] = "Other"
        malformed.write_text(json.dumps(wrong_schema), encoding="utf-8")
        check_fail(malformed, "schema validation failed")

        oversized = directory / "oversized.json"
        with oversized.open("wb") as output:
            output.truncate(MAX_REPORT_BYTES + 1)
        check_fail(oversized, "exceeds the")

        bad_target = directory / "preserved.sha256"
        bad_target.write_text("preserve this sidecar", encoding="ascii")
        malformed.write_text("{}", encoding="utf-8")
        invalid_export = run_verifier(malformed, "--write-digest", str(bad_target), "--force")
        assert invalid_export.returncode == 2
        assert bad_target.read_text(encoding="ascii") == "preserve this sidecar"

    print("Report verifier: schema, malformed JSON, size bounds, source hashes, sidecars, and atomic refusal passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
