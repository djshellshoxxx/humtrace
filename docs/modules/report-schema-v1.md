# HumTrace JSON report schema 1.3

## Purpose

Provide a stable, machine-readable export for the offline prototype. The schema is versioned and retains measurement units and analysis settings. It does not claim evidence integrity or chain of custody.

The JSON Schema definition is [`schema/analysis-report-v1.schema.json`](../../schema/analysis-report-v1.schema.json), using JSON Schema 2020-12.

## Structure

- `schema_version`: current string value `1.3` (`1.0`, `1.1` and `1.2` reports remain valid).
- `application`: `HumTrace`.
- `input`: source path and decoded sample rate, channels, bit depth, format code, and duration.
- `settings`: Hann window, frame size, hop, absolute dBFS threshold, local prominence threshold, (1.2) `peak_tracking` (peaks per frame, dynamic range, leakage exclusion bins, frequency tolerance, maximum gap frames, minimum observed frames), and (1.3) `welch_psd` (`null` when no PSD was requested; otherwise Hann window, segment and hop sizes, density unit, floor guard and radius in bins, maximum harmonics, and minimum PSD prominence, which follows `minimum_prominence_db`).
- `channels[]`: channel number, a `frames[]` timeline, (1.2) `tracks[]`, and (1.3) `psd`.
- Each frame contains start sample/time, strongest non-DC peak, and nominal 50/60 Hz measurements.
- Each harmonic measurement contains harmonic number, expected and measured bin frequency, level, local floor, prominence, and a threshold-support boolean.
- `tracks[]` (1.2): track number, first and last observed frame start (sample and seconds), observed and missing frame counts, mean/min/max frequency, mean/max level, first-to-last frequency change, and first-to-last drift rate (`null` when the span is zero). Each track lists its observations (frame start, frequency, level, prominence). Missing frames are counted, not interpolated. Each track has a null `source_classification`. Tracks are synthetic-tested only; their settings are unvalidated.
- `channels[].psd` (1.3): `null` when the file has no complete PSD segment; otherwise the averaged Welch PSD of that channel over every sample of the file: complete segments averaged, samples processed, bin width, broadband median density over non-DC, non-Nyquist bins, and `mains_50_hz_psd` / `mains_60_hz_psd`. Each series lists up to `max_harmonics` harmonics below Nyquist with expected and measured bin frequency (the stronger of the two bins bracketing the expected frequency), density, local floor (median over bins within `radius_bins`, excluding `guard_bins` each side), prominence and a `prominent` flag. The summary has a null `source_classification`. `prominent` only means prominence >= `minimum_prominence_db`; it is not a source classification. Synthetic-tested only.
- `interpretation`: null source classification and a statement that measurements, including tracks and PSD summaries, do not prove a physical source or cause.

## Encoding rules

- Non-finite floating-point values are encoded as JSON `null`; this is used for silence (`-inf` dBFS).
- File-path strings escape quotes, backslashes, control characters, and preserve UTF-8 bytes.
- Consumers must check `schema_version` and tolerate added fields in later minor revisions.
- Frame timestamps refer to window start; they do not identify the exact onset of a transient.

- Added in 1.3: `settings.welch_psd` and `channels[].psd` (additive).
- PSD densities use `density_unit` `dBFS^2/Hz` (10*log10 of one-sided power per hertz relative to 1 full-scale square per hertz), not peak-amplitude dBFS (`20*log10` of amplitude); do not compare the two scales directly. Prominence is a dB difference. `-inf` PSD values are encoded as `null`. Welch segments are Hann, complete segments only.
- Added in 1.2: `settings.peak_tracking` and `channels[].tracks[]` (additive; 1.0/1.1 consumers unaffected).
- Added in 1.1: top-level `application_version` and `warnings[]`; `input.sha256` and `input.size_bytes`.

## Current gaps

The report omits decoder/library versions, software build ID, user annotations, validation status, and a full error/warning list. Do not present it as a forensic provenance record until those fields and their handling are specified and validated.

## Migration

Use `include/humtrace/report.hpp` and `src/report.cpp` as a standalone module. Add the source to the consumer build, preserve the schema version and units, and port `tests/test_report.cpp`. A breaking field or semantic change requires a schema version change and compatibility policy.
