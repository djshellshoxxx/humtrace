# HumTrace JSON report schema 1.0

## Purpose

Provide a stable, machine-readable export for the offline prototype. The schema is versioned and retains measurement units and analysis settings. It does not claim evidence integrity or chain of custody.

The JSON Schema definition is [`schema/analysis-report-v1.schema.json`](../../schema/analysis-report-v1.schema.json), using JSON Schema 2020-12.

## Structure

- `schema_version`: current string value `1.1` (1.1 adds fields to 1.0; consumers tolerate added fields).
- `application`: `HumTrace`.
- `input`: source path (basename unless `--full-path`), byte size, SHA-256, whether size/mtime were unchanged during analysis, and decoded sample rate, channels, bit depth, format code, and duration.
- `settings`: Hann window, frame size, hop, absolute dBFS threshold, and local prominence threshold.
- `settings` also records the A4 reference, peak-detection and tracker parameters.
- `channels[]`: channel number, a `frames[]` timeline, and `tracks[]` (persistent peak tracks with raw observations, explicit gap counts, median frequency/level, least-squares drift, nearest note, end reason; ids are local to one analysis).
- `comparison` (optional): independent-summary comparison with a reference recording (hash, size, rate), same-index channel pairing, `alignment: none`, `resampling: none`, matches with deltas, and unmatched track ids.
- Each frame contains start sample/time, strongest non-DC peak, and nominal 50/60 Hz measurements.
- Each harmonic measurement contains harmonic number, expected and measured bin frequency, level, local floor, prominence, and a threshold-support boolean.
- `interpretation`: null source classification and a statement that measurements do not prove a physical source.

## Encoding rules

- Non-finite floating-point values are encoded as JSON `null`; this is used for silence (`-inf` dBFS).
- File-path strings escape quotes, backslashes, control characters, and preserve UTF-8 bytes.
- Consumers must check `schema_version` and tolerate added fields in later minor revisions.
- Frame timestamps refer to window start; they do not identify the exact onset of a transient.

## Current gaps

The report omits decoder/library versions, software build ID, user annotations, validation status, and a full error/warning list. Do not present it as a forensic provenance record until those fields and their handling are specified and validated.

## Migration

Use `include/humtrace/report.hpp` and `src/report.cpp` as a standalone module. Add the source to the consumer build, preserve the schema version and units, and port `tests/test_report.cpp`. A breaking field or semantic change requires a schema version change and compatibility policy.
