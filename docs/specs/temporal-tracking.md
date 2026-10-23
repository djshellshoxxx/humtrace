# Temporal peak tracking

## Goal and current status

Associate measured local peaks across ordered frames within one channel and
produce reviewable track segments with explicit gaps and frequency/level
summaries. This is descriptive analysis. It does not identify a physical
source or establish that a spectral component physically split, merged, or
continued through an ambiguous crossing.

The implementation now records candidate ambiguity relationships for
one-to-many, many-to-one, and many-to-many connected candidate components.
These are conservative graph classifications that end prior track IDs and
start new ones. Physical split/merge lineage, event coverage classification,
calibrated persistence labels, sustained-jump classification, and validation on
held-out recordings remain incomplete.

## Implemented input and output

Input to the tracker is an ordered `FramePeaks` sequence for one channel, the
sample rate, and settings. A frame supplies its start sample and peaks with
frequency and level. The pipeline currently supplies at most eight prominent
local maxima per frame (minimum level -80 dBFS, prominence 10 dB over the local
floor); it runs tracking independently per channel.

`track_peaks` returns tracks with analysis-local IDs, actual observations,
first/last frame, observed and missing-frame counts, median and quartile
frequency/level, drift slope, and optional drift fit summaries. A track's nested
`event` summary repeats the local ID, inclusive frame bounds, observed count,
and explicit missing-frame ranges. It is a summary of one track segment, not a
separately classified persistent event. Observations hold frame index, frame
start sample, frequency, and level; richer spectral evidence remains in the
analysis frame records.

The optional ambiguity output records local event ID, frame index, start sample,
graph kind (`split`, `merge`, or `competing`), parent local track IDs, and child
local track IDs. Tracks also carry optional start/end ambiguity IDs and an end
reason (`end_of_input`, `gap_exceeded`, or `ambiguous`). IDs restart per
channel/analysis and can change when data or settings change. Events are emitted
even when the associated tracks are later removed by the minimum-observation
filter, so an event may refer to a suppressed track ID. The channel reports a
suppressed-track count.

## Candidate graph association

For each current valid peak and each active prior track, connect an edge when
the absolute frequency difference is within:

`max(fixed_hz_tolerance, relative_tolerance * prior_frequency, bin_fraction * bin_spacing_hz)`

The comparison uses the prior track's last observed frequency. All three terms
are settings and serialized with the analysis, but this radius is not a
calibrated uncertainty bound.

Only a connected component containing exactly one prior track and one current
peak continues the previous ID. Every other connected component that contains
both a prior track and current peak ends all its parents as ambiguous and starts
a fresh child ID for each current peak. One parent with multiple peaks is
classified `split`; multiple parents with one peak is `merge`; multiple parents
and multiple peaks is `competing`. Level, width, and edge distance remain
measurements and do not select an identity inside a connected component.

Consequently, a candidate “split” or “merge” describes only the shape of one
frame's tolerance graph. It makes no claim about a real source splitting or
merging. The system does not preserve a physical lineage through the event.
Out-of-radius peaks begin unlinked tracks; there is no sustained-jump classifier
or automatic linkage across those segments. Missing observations are not
interpolated. A prior endpoint survives up to `max_gap_frames` missing frames
(default one); after a longer gap it ends as `gap_exceeded`.

IDs are deterministic: parent IDs are ascending, child IDs follow the input
order of valid peaks, and event IDs follow the smallest parent ID in each
component. Event callbacks happen before end callbacks for parents and
observation callbacks for children, allowing a disk-backed consumer to record
references before summarizing a segment. `PeakAssociator` defaults to 4,096
active tracks and supports a configured maximum of 65,536. The pipeline applies
its active-track cap across channels. An in-radius edge cap of 1,048,576 applies
per frame; exceeding either resource bound fails explicitly. Scratch memory
retains active endpoints, graph nodes, and components but not the edge list.
Pair-check time still scales with the product of active tracks and valid peaks.

## Suppression and wording

`min_observed_frames` defaults to three. Shorter tracks are omitted from the
reported track list after candidate events and local IDs have been assigned;
they increment the suppressed count. This is a noise-flicker control, not a
validated duration or coverage threshold. A report may therefore contain an
ambiguity event whose parent or child is not present among reported tracks.

Until held-out corpus evaluation establishes persistence behavior, user-facing
wording should state observations and counts (for example, “track observed in N
of M frames”). Missing harmonics remain missing or weak evidence; no
measurement is inferred through a gap. Avoid interpreting a candidate graph
relationship as cause, source identity, or physical split/merge behavior.

## Validation still required

Automated tracker tests exercise gaps, isolated matches, candidate split/merge/
competing cases, unequal measurement levels, deterministic IDs, invalid peaks,
active-track limits, and the candidate-edge cap. They do not establish field
performance. Before treating association or persistence rules as validated,
measure results on labeled recordings separated by source/session. Include
crossings and close tones across SNR and frequency resolution, amplitude
modulation, dropouts, flickering noise peaks, abrupt jumps, and channel swaps.
Report identity switches, fragmentation, false ambiguity and merge rates, as
well as boundary error and maximum-count runtime. Validate any tolerance,
minimum duration, or coverage threshold on data held out from tuning.

## Module boundary

Keep association in the standard-library tracker module and consume measured
per-channel peaks without depending on GUI plotting or JSON. The streaming
consumer stores observations and ambiguity tables on disk and exposes bounded
random-access track summaries. Track and ambiguity IDs remain analysis-local;
project/session-level stable event identity is not implemented. See the
[analysis engine](analysis-engine.md), [validation corpus](validation-corpus.md),
and [tracker module contract](../modules/tracker.md).
