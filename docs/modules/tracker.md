# Tracker module

## Purpose

Associate prominent per-frame spectral peaks within one channel into analysis-
local track segments. Summaries describe observed frequency/level, gaps, and
drift. They do not identify a physical source, and ambiguity events do not
establish that a tone physically split, merged, or continued through a crossing.
See [temporal tracking](../specs/temporal-tracking.md).

## Files and API

- `include/humtrace/tracker.hpp`, `src/tracker.cpp`; standard library only, with
  no JSON or GUI dependency.
- `FramePeaks` carries a frame start sample and frequency/level peak pairs.
  `track_peaks(frames, sample_rate_hz, settings, suppressed, ambiguities)`
  returns retained `Track` summaries. The optional ambiguity output is cleared
  first and then receives this channel's frame-local candidate relationships.
- `PeakAssociator::push(frame, observed, ended, event)` supports incremental
  consumers. Its event callback runs before end callbacks for parent segments
  and observation callbacks for newly created child segments. The event callback
  is optional.
- `TrackAmbiguity` reports an analysis-local ID, frame index, frame start sample,
  `Split`, `Merge`, or `Competing` graph shape, and parent/child local track IDs.
  `Track` has optional start/end ambiguity IDs and an end reason
  (`EndOfInput`, `GapExceeded`, or `Ambiguous`). These IDs are local to one
  channel analysis and are not stable across settings or runs.
- Report JSON contains per-track observations, missing-frame ranges, median and
  quartile frequency/level, drift and fit summaries, end reason, and ambiguity
  references. Each channel's ambiguity table retains events even if a parent or
  child track is later suppressed from the reported track list; an event may
  therefore reference a suppressed local ID. The per-channel suppressed count
  reports how many short tracks were omitted.

## Association and bounds

- The pipeline supplies up to `max_peaks_per_frame` (default 8) prominent local
  maxima from `find_peaks` (minimum level -80 dBFS and 10 dB above local floor).
  Direct callers may supply more, subject to the active-track bound.
- For each active track and valid current peak, an edge exists when their
  absolute frequency difference is no more than
  `max(fixed_hz_tolerance, relative_tolerance * prior_frequency,
  bin_fraction * bin_spacing_hz)`. The comparison uses the track's last
  observed frequency. Settings are serialized; this radius has not been
  calibrated as an uncertainty interval.
- Only a connected graph component with exactly one prior track and one current
  peak preserves the track ID. Every other connected component that contains
  both sides ends all its prior tracks with `Ambiguous` and begins a fresh track
  for each current peak. A 1:N component is labeled `Split`, N:1 `Merge`, and
  N:M `Competing`. Unequal distances, levels, or widths do not resolve a
  connected component. Candidate labels describe graph shape only; they are
  not physical split/merge lineage.
- Parent IDs are ascending, child IDs follow valid input peak order, and event
  IDs follow ascending smallest-parent ID within the frame. IDs restart for
  each channel analysis. Out-of-radius peaks start unlinked tracks; the tracker
  has no sustained-jump classifier.
- A track can survive up to `max_gap_frames` consecutive missing observations
  (default 1). Missing samples are never interpolated. `missing_frames` counts
  only gaps between the first and last actual observations. After a longer gap,
  the old segment ends as `GapExceeded` and a later peak starts a new segment.
- Tracks with fewer than `min_observed_frames` observations (default 3) are
  removed from the reported track list after association. This suppresses short
  tracks but is not a corpus-validated persistence rule. Candidate events are
  retained even when referenced tracks are suppressed.
- `PeakAssociator` defaults to at most 4,096 active tracks; its constructor
  accepts up to 65,536. The pipeline applies its configured active-track budget
  across channels. At most 1,048,576 in-radius candidate edges are accepted per
  frame; exceeding the edge cap or active bound fails explicitly. Scratch graph
  storage is bounded by active endpoints and valid peaks rather than retaining
  candidate edges. Pair checks still scale with active tracks times valid
  peaks, so maximum-count runtime needs benchmarking.
- Non-finite, zero, or negative-frequency peaks are ignored. Invalid settings
  or sample rate throw `std::invalid_argument`; active/edge resource-limit
  violations throw `std::runtime_error`.

## Boundaries

The API preserves local candidate relationships, not physical identity or
long-term lineage. It has no event coverage threshold, validated persistence
label, sustained frequency-jump detector, uncertainty estimate, or resolution
for connected ambiguity. A conservative graph may fragment close tracks. No
“persistent” source or cause wording is justified by this module alone.

## Tests

`tests/test_tracker.cpp` covers gaps, isolated continuation, split/merge/
competing candidate components, unequal-evidence ambiguity, deterministic ID
ordering, invalid peaks/settings, active-track limits, and the candidate-edge
cap. Streaming/report tests cover retained event output and parity. Held-out
recording validation, identity-switch and fragmentation rates, false ambiguity
rates, and maximum-count runtime remain open.
