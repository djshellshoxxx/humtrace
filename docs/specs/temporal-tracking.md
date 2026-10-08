# Persistent peak tracking and event specification

## Goal

Associate measured local peaks across adjacent frames into repeatable tracks and convert tracks into reviewable event intervals. This describes persistence and drift; it does not identify a physical source.

## Track input/output

Input: ordered per-channel frame peak lists, settings, sample rate, and frame/hop positions. Output: deterministic track IDs local to one analysis, observations, event intervals, gaps, and end reason. Keep raw per-frame observations even when an event is summarized.

Each observation contains sample/time bounds, raw and refined frequency, peak/RMS level, PSD, local floor/prominence, width, and validity flags. Each track summary contains start/end, observed-frame count, missing-frame count, median/quantile frequency and level, drift estimate with fit quality, and per-observation references.

## Association algorithm v0.1

- Match only within one channel. Candidate cost uses absolute frequency distance normalized by expected measurement uncertainty, level discontinuity, and width change. Frequency dominates; level is a tie breaker.
- Maximum frequency distance is `max(fixed_hz_tolerance, relative_tolerance * prior_frequency, validated_bin_fraction * bin_spacing)`. All terms are settings and are serialized.
- Use deterministic one-to-one assignment per frame. If a full assignment solver is not justified by measured peak counts, use sorted greedy nearest-match with tie-breaking on prior track ID and measured bin. Benchmark the max candidate count.
- A track may survive a configurable short gap (default one missing frame for prototype evaluation). Keep the gap explicit; do not interpolate a measurement into it. Longer gaps start a new event unless validated data supports another rule.
- Split tracks on sustained frequency jumps, competing-peak ambiguity, or a time gap beyond the configured limit. Preserve split/merge lineage so UI does not hide ambiguity.
- Assign stable event IDs only when saving a project/session. Analysis-local IDs may change when settings change and must be labeled as such.

## Event thresholds and wording

Minimum observed duration, coverage ratio, and supporting evidence thresholds are configurable and stored. A “persistent candidate” label is allowed only if its threshold has been evaluated on the validation corpus. Before that, UI wording is descriptive (“track observed in N of M frames”). Missing harmonics are reported as missing/weak evidence; they are not inferred.

## Acceptance tests

Synthetic cases: exact tone, slow drift, amplitude modulation, dropout gaps, two tones crossing, close tones, noise peaks that flicker, abrupt frequency jump, channel-swapped tones, and low-SNR intervals. Require deterministic output for identical input/settings, no channel crossing, correct event boundaries within one hop, no interpolation through gaps, and reproducible split/merge decisions. Report identity switches and fragmentation/merge rates on labeled recordings.

## Migration boundary

Keep association in a standalone `analysis/tracker` module consuming the analysis engine's public peak observations. It must not depend on GUI plotting or JSON. Export a compact immutable event model that can be used by comparison, UI, and reports. See [analysis engine](analysis-engine.md) and [validation corpus](validation-corpus.md).
