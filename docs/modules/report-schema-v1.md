# HumTrace JSON report schema 1.3

## Purpose

Provide a stable, machine-readable export for the offline measurement application. Schema 1.3 retains measurement units and effective analysis settings. It does not claim evidence integrity or chain of custody.

The JSON Schema definition is [`schema/analysis-report-v1.schema.json`](../../schema/analysis-report-v1.schema.json), using JSON Schema 2020-12.
The source-tree [`verify_report.py`](../../tools/verify_report.py) utility checks this schema with `jsonschema`, rejects malformed or duplicate-key JSON, caps report input at 64 MiB, and optionally compares input and report-file SHA-256 digests. Its hashes identify bytes only; it is not a certificate or chain-of-custody verifier, and desktop report import is not implemented.

## Structure

- `schema_version`: current string value `1.3`. Version 1.2 added generator and pipeline metadata, input byte length, warnings, and per-channel suppression/clipping counts. Version 1.3 records effective detector and Welch settings, refined-peak resolution/quality fields, track fit statistics and observed intervals, and channel-local candidate ambiguity events and references.
- `application`: `HumTrace`.
- `input`: source path, SHA-256 of the exact input bytes (`null` if not computed), and decoded sample rate, channels, bit depth, format code, and duration.
- `settings`: Hann window, frame/hop, thresholds, tracker policy, peak-search settings, and Welch PSD settings.
- `channels[]`: channel number, `frames[]`, `tracks[]`, `suppressed_tracks`, `clipped_samples`, optional PSD summary, and `ambiguities[]`. Tracks contain analysis-local IDs, observations, quantiles, drift fit quality, observed intervals/gaps, end reason, and references to ambiguity events. Gaps are counted, never interpolated; these are descriptive segments, not source attribution.
- Each frame contains start sample/time, strongest non-DC peak measurements and quality/resolution metadata, and nominal 50/60 Hz measurements.
- Each harmonic measurement contains harmonic number, expected and measured bin frequency, level, local floor, prominence, and a threshold-support boolean.
- `interpretation`: null source classification and a statement that measurements do not prove a physical source.

JSON numbers are locale-independent and absent peak frequencies are null. Frequency
uncertainty remains null while uncharacterized; resolution and width metadata are
not error bounds. Candidate ambiguity tables retain analysis-local parent/child
references even when short segments are suppressed. These graph relationships do
not establish physical source identity. Input hashes identify bytes; a bounded
second hash checks persistent file edits but does not establish an immutable
snapshot or authenticity. Comparison exports retain lightweight per-input settings
snapshots.

## Encoding rules

- Non-finite floating-point values are encoded as JSON `null`; this is used for silence (`-inf` dBFS).
- File-path strings escape quotes, backslashes, control characters, and preserve UTF-8 bytes.
- Consumers must check `schema_version` and tolerate added fields in later minor revisions.
- Frame timestamps refer to window start; they do not identify the exact onset of a transient.

## Current gaps

The report omits decoder/library versions, software build ID, user annotations, validation status, and a full error/warning list. Do not present it as a forensic provenance record until those fields and their handling are specified and validated.

## Migration

Use `include/humtrace/report.hpp` and `src/report.cpp` as a standalone module. Add the source to the consumer build, preserve the schema version and units, and port `tests/test_report.cpp`. A breaking field or semantic change requires a schema version change and compatibility policy.
