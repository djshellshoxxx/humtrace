# HumTrace JSON report schema (1.0 - 1.2) and comparison schema 1.0

## Purpose

Provide a stable, machine-readable export for the offline prototype. The schema is versioned and retains measurement units and analysis settings. It does not claim evidence integrity or chain of custody.

The JSON Schema definition is [`schema/analysis-report-v1.schema.json`](../../schema/analysis-report-v1.schema.json), using JSON Schema 2020-12.

## Structure

- `schema_version`: current string value `1.2` (`1.0` and `1.1` reports remain valid; `tracks` and `settings.tone_tracking` are optional in the schema for them).
- `application`: `HumTrace`.
- `input`: source path and decoded sample rate, channels, bit depth, format code, and duration.
- `settings`: Hann window, frame size, hop, absolute dBFS threshold, and local prominence threshold.
- `channels[]`: channel number, a `frames[]` timeline and (1.2) `tracks[]`.
- Each frame contains start sample/time, strongest non-DC peak, and nominal 50/60 Hz measurements.
- Each harmonic measurement contains harmonic number, expected and measured bin frequency, level, local floor, prominence, and a threshold-support boolean.
- `interpretation`: null source classification and a statement that measurements do not prove a physical source.

## Encoding rules

- Non-finite floating-point values are encoded as JSON `null`; this is used for silence (`-inf` dBFS).
- File-path strings escape quotes, backslashes, control characters, and preserve UTF-8 bytes.
- Consumers must check `schema_version` and tolerate added fields in later minor revisions.
- Frame timestamps refer to window start; they do not identify the exact onset of a transient.

- Added in 1.2: `settings.tone_tracking` (`max_peaks_per_frame`, `frequency_tolerance_hz`, `max_gap_frames`, `min_frames` (effective: max of 5 frames and 0.5 s of hops), `truncated`) and per-channel `tracks[]`. Each track has `start_sample`/`end_sample`, `start_seconds`/`end_seconds`, `frames_detected`, `frames_spanned`, `persistence`, `median`/`min`/`max_frequency_hz`, `drift_hz_per_second`, `median_level_dbfs`, `max_level_dbfs`, `median_prominence_db`, and `near_mains_multiple` (null or `{nominal_hz, harmonic, deviation_hz}`; nearest n x 50/60 Hz, n <= 10). Any number may be `null`. `truncated: true` means track limits (64 active, 4096 total) dropped candidates.
- `near_mains_multiple` is frequency proximity only; it is not a source claim.
- Added in 1.1: top-level `application_version` and `warnings[]`; `input.sha256` and `input.size_bytes`.

## Comparison report

[`schema/comparison-report-v1.schema.json`](../../schema/comparison-report-v1.schema.json), written by `humtrace --compare a.wav b.wav --json out.json` and the GUI compare dialog. Fields: `comparison_schema_version` (`1.0`), `application`, `application_version`, `files[]` (1-based `index`, `path`, `sha256`, `sample_rate_hz`, `frame_size_samples`, `track_tolerance_hz`, per-channel summaries with `track_frequencies_hz`), `shared_tones[]` (`frequency_hz`, 1-based `files`, `near_mains_multiple`), `settings` and `interpretation` (null `source_classification`). A shared tone means similar tracked frequencies, not a common source.

## Current gaps

The report omits decoder/library versions, software build ID, user annotations, validation status, and a full error/warning list. Do not present it as a forensic provenance record until those fields and their handling are specified and validated.

## Migration

Use `include/humtrace/report.hpp` and `src/report.cpp` as a standalone module. Add the source to the consumer build, preserve the schema version and units, and port `tests/test_report.cpp`. A breaking field or semantic change requires a schema version change and compatibility policy.
