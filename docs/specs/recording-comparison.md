# Recording comparison specification

## Scope

Compare measurements from two or more recordings to help an analyst inspect repeated tones, drift, persistence, and channel distribution. Similarity is not location identification, authenticity, common-device attribution, or proof that recordings share an environment.

## Comparison modes

1. **Independent summary:** compare per-file metadata, detected track summaries, frequency, level, persistence, and channels without time alignment.
2. **Explicit timestamp alignment:** user chooses a reference offset; record offset and source of choice.
3. **Signal-assisted alignment:** optional later feature based on a documented correlation target and search range. Show original and chosen offset, score surface, ambiguity, and uncertainty. Never apply alignment silently.
4. **Cross-spectral comparison:** only for compatible channel signals with common sample-rate/clock assumptions. Report coherence, cross-spectrum, phase convention, frequency band and estimator settings. A high coherence indicates a stable relationship between recorded channels, not physical coupling route.

## Resampling and compatibility

By default, do not resample. Show sample rate, duration, channel layout, bit depth, and decoder transformations. If users request comparison in a common rate, create a derived analysis view using a named, versioned resampler and record input/output rates, filter, delay compensation, and boundary handling. Original files remain untouched. Do not equate channel 1 across devices unless the user maps channels.

## UI and report

Comparison view overlays or juxtaposes tracks with line style and labels in addition to color. A table lists Δfrequency, Δlevel, duration, overlap, and uncertainty. Missing matches appear as unmatched; do not score as zero similarity. Export includes all compared file hashes, selected channels, alignment mode/offset, resampling settings, method version, exclusions, and warnings.

## Tests

Test identical signals, known offsets, different rates, resampling, reversed polarity, gain differences, unrelated noise, repeated periodic signals with ambiguous lag, partial overlap, and unequal durations. Verify that unrequested alignment/resampling never occurs, ambiguous correlation is surfaced, and reports reproduce all transformations. Defer categorical similarity thresholds until held-out recordings support them.

## Reuse

Implement a pure `compare` module over immutable analysis/event records. It must not load files, change decoder state, or depend on GUI. Consume [temporal tracks](temporal-tracking.md), emit a versioned comparison model, and expose its methods to both UI and report serializer.
