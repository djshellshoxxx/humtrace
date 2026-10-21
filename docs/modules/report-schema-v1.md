# HumTrace JSON report schema 1.2

## Purpose

Provide a stable, machine-readable export for the offline prototype. The schema is versioned and retains measurement units and analysis settings. It does not claim evidence integrity or chain of custody.

The JSON Schema definition is [`schema/analysis-report-v1.schema.json`](../../schema/analysis-report-v1.schema.json), using JSON Schema 2020-12.

## Structure

- `schema_version`: current string value `1.2`. Version 1.1 added `settings.peak_tracking` and `channels[].tracks[]`. Version 1.2 adds `settings.welch_psd` and `channels[].psd`; all 1.0 and 1.1 fields are unchanged.
- `application`: `HumTrace`.
- `input`: source path and decoded sample rate, channels, bit depth, format code, and duration.
- `settings`: Hann window, frame size, hop, absolute dBFS threshold, local prominence threshold, `peak_tracking` (peaks per frame, dynamic range, leakage exclusion bins, frequency tolerance, maximum gap frames, minimum observed frames), and `welch_psd` (`null` when no PSD was requested; otherwise Hann window, segment and hop sizes, density unit, floor guard and radius in bins, maximum harmonics, and minimum PSD prominence).
- `channels[]`: channel number, a `frames[]` timeline, and `tracks[]`.
- Each frame contains start sample/time, strongest non-DC peak, and nominal 50/60 Hz measurements.
- Each harmonic measurement contains harmonic number, expected and measured bin frequency, level, local floor, prominence, and a threshold-support boolean.
- `tracks[]`: track number, first and last observed frame start (sample and seconds), observed and missing frame counts, mean/min/max frequency, mean/max level, first-to-last frequency change, and first-to-last drift rate (`null` when the span is zero). Each track lists its observations (frame start, frequency, level, prominence). Missing frames are counted, not interpolated. Each track carries a null `source_classification`.
- `channels[].psd`: `null` when the file has no complete PSD segment; otherwise the averaged Welch PSD summary of that channel. It holds the number of complete segments averaged, samples processed, bin width, the broadband median density over non-DC, non-Nyquist bins, and `mains_50_hz_psd` and `mains_60_hz_psd`. Each nominal series lists up to `max_harmonics` harmonics below Nyquist with the expected and measured bin frequency, the measured density, the local floor, prominence, and a `prominent` flag. The measured bin is the stronger of the two bins bracketing the expected frequency. The local floor is the median density over bins within `radius_bins` of the harmonic, excluding `guard_bins` on each side. The PSD summary carries a null `source_classification`. `prominent` means only that prominence is at least `minimum_prominence_db`; it is not a source classification.
- `interpretation`: null source classification and a statement that measurements, including tracks and PSD summaries, do not prove a physical source.

## Encoding rules

- Non-finite floating-point values are encoded as JSON `null`; this is used for silence (`-inf` dBFS and `-inf` dBFS^2/Hz).
- PSD densities use `density_unit` `dBFS^2/Hz`, defined as 10*log10 of one-sided power per hertz relative to 1 full-scale square per hertz. They are not peak-amplitude dBFS values (`20*log10` of amplitude), so do not compare the two scales directly. Harmonic prominence is a difference in dB and is the same quantity on both scales for a given component.
- Welch PSD windows are Hann, use the symmetric window definition of the peak-amplitude spectrum, and average complete segments only; a trailing partial segment is excluded and counted in no segment.
- File-path strings escape quotes, backslashes, control characters, and preserve UTF-8 bytes.
- Consumers must check `schema_version` and tolerate added fields in later minor revisions.
- Frame timestamps refer to window start; they do not identify the exact onset of a transient.

## Current gaps

The report omits input hashes, decoder/library versions, software build ID, user annotations, validation status, and a full error/warning list. Do not present it as a forensic provenance record until those fields and their handling are specified and validated.

## Migration

Use `include/humtrace/report.hpp` and `src/report.cpp` as a standalone module. Add the source to the consumer build, preserve the schema version and units, and port `tests/test_report.cpp`. A breaking field or semantic change requires a schema version change and compatibility policy.
