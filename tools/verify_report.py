#!/usr/bin/env python3
"""Validate a HumTrace 1.3 report and optionally compare its source bytes.

This source-tree utility validates report structure and byte digests. It does not
authenticate the recorder, report author, acquisition time, or chain of custody.
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import math
import os
import re
import stat
import sys
import tempfile
from pathlib import Path
from typing import Any


MAX_REPORT_BYTES = 64 * 1024 * 1024
HASH_CHUNK_BYTES = 1024 * 1024
SCHEMA_PATH = Path(__file__).resolve().parent.parent / "schema" / "analysis-report-v1.schema.json"


class VerificationError(Exception):
    pass


def _stable_stat(before: os.stat_result, after: os.stat_result) -> bool:
    return (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) == (
        after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns
    )


def _require_regular_file(path: Path, descriptor: int) -> os.stat_result:
    info = os.fstat(descriptor)
    if not stat.S_ISREG(info.st_mode):
        raise VerificationError(f"{path}: expected a regular file")
    return info


def _read_report(path: Path) -> bytes:
    try:
        with path.open("rb") as source:
            before = _require_regular_file(path, source.fileno())
            if before.st_size > MAX_REPORT_BYTES:
                raise VerificationError(
                    f"{path}: report exceeds the {MAX_REPORT_BYTES}-byte validation limit"
                )
            data = source.read(MAX_REPORT_BYTES + 1)
            after = os.fstat(source.fileno())
    except OSError as error:
        raise VerificationError(f"{path}: {error.strerror or error}") from error
    if len(data) > MAX_REPORT_BYTES:
        raise VerificationError(f"{path}: report exceeds the {MAX_REPORT_BYTES}-byte validation limit")
    if len(data) != after.st_size or not _stable_stat(before, after):
        raise VerificationError(f"{path}: report changed while it was being read")
    return data


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise VerificationError(f"JSON contains duplicate object key {key!r}")
        result[key] = value
    return result


def _reject_nonfinite(value: str) -> None:
    raise VerificationError(f"JSON contains non-finite number {value}")


def _parse_finite_float(value: str) -> float:
    parsed = float(value)
    if not math.isfinite(parsed):
        raise VerificationError(f"JSON number {value} is outside the finite numeric range")
    return parsed


def _load_schema() -> tuple[dict[str, Any], Any]:
    try:
        import jsonschema
    except ImportError as error:
        raise VerificationError("the report verifier requires Python package 'jsonschema'") from error
    try:
        schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
        jsonschema.Draft202012Validator.check_schema(schema)
    except (OSError, UnicodeError, json.JSONDecodeError, jsonschema.SchemaError) as error:
        raise VerificationError(f"could not load the published report schema: {error}") from error
    return schema, jsonschema


def _parse_and_validate(data: bytes, schema: dict[str, Any], jsonschema: Any) -> dict[str, Any]:
    try:
        text = data.decode("utf-8", errors="strict")
        report = json.loads(
            text,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_nonfinite,
            parse_float=_parse_finite_float,
        )
    except (UnicodeDecodeError, ValueError, VerificationError, RecursionError, MemoryError) as error:
        raise VerificationError(f"invalid report JSON: {error}") from error
    if not isinstance(report, dict):
        raise VerificationError("invalid report JSON: the top-level value must be an object")
    version = report.get("schema_version")
    if version != "1.3":
        raise VerificationError(f"unsupported report schema version {version!r}; this verifier accepts 1.3")
    validator = jsonschema.Draft202012Validator(schema)
    try:
        issue = next(validator.iter_errors(report), None)
    except (RecursionError, MemoryError) as error:
        raise VerificationError("report exceeds the supported validation complexity") from error
    if issue is not None:
        location = ".".join(str(item) for item in issue.absolute_path) or "$"
        raise VerificationError(f"schema validation failed at {location}: {issue.message}")
    return report


def _hash_input(path: Path, expected_bytes: int) -> str:
    try:
        with path.open("rb") as source:
            before = _require_regular_file(path, source.fileno())
            if before.st_size != expected_bytes:
                raise VerificationError(
                    f"{path}: byte length is {before.st_size}, report records {expected_bytes}"
                )
            digest = hashlib.sha256()
            count = 0
            while True:
                chunk = source.read(HASH_CHUNK_BYTES)
                if not chunk:
                    break
                digest.update(chunk)
                count += len(chunk)
            after = os.fstat(source.fileno())
    except OSError as error:
        raise VerificationError(f"{path}: {error.strerror or error}") from error
    if count != expected_bytes or not _stable_stat(before, after):
        raise VerificationError(f"{path}: input changed while its bytes were being checked")
    return digest.hexdigest()


def _paths_refer_to_same_file(first: Path, second: Path) -> bool:
    try:
        return os.path.samefile(first, second)
    except OSError:
        try:
            return first.resolve() == second.resolve()
        except (OSError, RuntimeError) as error:
            raise VerificationError("could not safely resolve report, input, or digest paths") from error


def _read_digest(path: Path) -> str:
    try:
        with path.open("rb") as source:
            before = _require_regular_file(path, source.fileno())
            contents = source.read(66)
            after = os.fstat(source.fileno())
    except OSError as error:
        raise VerificationError(f"{path}: {error.strerror or error}") from error
    if not _stable_stat(before, after):
        raise VerificationError(f"{path}: digest file changed while it was being read")
    if len(contents) > 65 or not re.fullmatch(rb"[0-9a-f]{64}\n?", contents):
        raise VerificationError(f"{path}: expected 64 lowercase hexadecimal characters and an optional newline")
    return contents[:64].decode("ascii")


def _write_digest(path: Path, digest: str, force: bool) -> None:
    if not path.parent.is_dir():
        raise VerificationError(f"{path}: output directory does not exist")
    if path.exists() and not force:
        raise VerificationError(f"{path}: refusing to replace an existing digest file (use --force)")
    temporary: str | None = None
    try:
        descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.tmp-", dir=path.parent)
        with os.fdopen(descriptor, "wb") as destination:
            destination.write((digest + "\n").encode("ascii"))
            destination.flush()
            os.fsync(destination.fileno())
        if force:
            os.replace(temporary, path)
        else:
            # Linking a fully written sibling temp file publishes without clobbering a race.
            os.link(temporary, path)
            try:
                os.unlink(temporary)
            except OSError:
                pass
        temporary = None
    except FileExistsError as error:
        raise VerificationError(f"{path}: refusing to replace an existing digest file (use --force)") from error
    except OSError as error:
        raise VerificationError(f"{path}: could not publish digest: {error.strerror or error}") from error
    finally:
        if temporary is not None:
            try:
                os.unlink(temporary)
            except OSError:
                pass


def _run(arguments: argparse.Namespace) -> None:
    report_path = Path(arguments.report)
    data = _read_report(report_path)
    schema, jsonschema = _load_schema()
    report = _parse_and_validate(data, schema, jsonschema)
    report_digest = hashlib.sha256(data).hexdigest()

    input_path = Path(arguments.input) if arguments.input else None
    digest_path = Path(arguments.digest_file) if arguments.digest_file else None
    output_digest_path = Path(arguments.write_digest) if arguments.write_digest else None
    selected_paths = [report_path] + [
        path for path in (input_path, digest_path, output_digest_path) if path is not None
    ]
    for index, first in enumerate(selected_paths):
        for second in selected_paths[index + 1:]:
            if _paths_refer_to_same_file(first, second):
                raise VerificationError("report, input, and digest paths must identify different files")
    if digest_path is not None:
        expected_report_digest = _read_digest(digest_path)
        if not hmac.compare_digest(report_digest, expected_report_digest):
            raise VerificationError(f"{report_path}: report-file SHA-256 does not match {digest_path}")
    if input_path is not None:
        source = report["input"]
        if not isinstance(source["bytes"], int) or isinstance(source["bytes"], bool):
            raise VerificationError("report input byte length is not an integer")
        if not isinstance(source["sha256"], str):
            raise VerificationError("report has no input SHA-256 to compare")
        actual_input_digest = _hash_input(input_path, source["bytes"])
        if not hmac.compare_digest(actual_input_digest, source["sha256"]):
            raise VerificationError(f"{input_path}: SHA-256 does not match the report")
        print(f"Input bytes match report: {actual_input_digest}")
    if output_digest_path is not None:
        _write_digest(output_digest_path, report_digest, arguments.force)
    elif arguments.force:
        raise VerificationError("--force requires --write-digest")

    print("Schema valid: HumTrace analysis report 1.3")
    print(f"Report-file SHA-256 (computed): {report_digest}")
    if digest_path is not None:
        print(f"Report-file SHA-256 matches sidecar: {digest_path}")
    if output_digest_path is not None:
        print(f"Wrote report-file SHA-256 sidecar: {output_digest_path}")
    print("Digest checks identify matching bytes; they do not authenticate origin, authorship, or custody.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", help="HumTrace report JSON file (maximum 64 MiB)")
    parser.add_argument("--input", help="optional source file to compare with the report's recorded byte length and SHA-256")
    parser.add_argument("--digest-file", help="optional report SHA-256 sidecar to verify")
    parser.add_argument("--write-digest", help="write a detached SHA-256 sidecar after successful validation")
    parser.add_argument("--force", action="store_true", help="atomically replace an existing digest sidecar")
    arguments = parser.parse_args()
    try:
        _run(arguments)
    except VerificationError as error:
        print(f"verify_report.py: error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
