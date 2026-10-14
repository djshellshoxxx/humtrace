# Tracker module

## Purpose

Associate per-frame spectral peaks of one channel into tracks and summarize persistence and drift, per [temporal tracking](../specs/temporal-tracking.md). Descriptive only: no source attribution.

## Files and API

- `include/humtrace/tracker.hpp`, `src/tracker.cpp`; standard library only; no JSON or GUI dependency.
- `track_peaks(frames, sample_rate_hz, settings)` returns `Track` records (analysis-local id, raw observations, observed/missing frame counts, median frequency/level, least-squares drift in Hz/s).

## Contract

- Match radius = `max(fixed_hz_tolerance, relative_tolerance * prior_frequency, bin_fraction * bin_spacing_hz)`; all settings are serialized in the report.
- Deterministic greedy one-to-one assignment sorted by (distance, track id, peak index).
- A track survives up to `max_gap_frames` consecutive empty frames; the gap is counted in `missing_frames`, never filled. Longer gaps start a new track.
- Non-finite, zero, or negative-frequency peaks are ignored. Invalid settings throw `std::invalid_argument`.
- Pipeline input is up to `max_peaks_per_frame` (default 8) prominent local maxima per frame from `find_peaks` (min level -80 dBFS, 10 dB over local floor), so concurrent tones form separate tracks.

## Not yet implemented

Split/merge lineage, sustained-jump and competing-peak ambiguity flags, event intervals with coverage thresholds, and corpus-validated persistence labels. No "persistent" wording is permitted until validated.

## Tests

`tests/test_tracker.cpp`: dropout gap, gap split, two-tone separation, frequency jump, invalid peaks, determinism, invalid settings. Pipeline/CLI coverage in `tests/test_cli.py`.
