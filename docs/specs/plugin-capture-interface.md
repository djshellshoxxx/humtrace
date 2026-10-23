# Proposed plugin capture and analysis interface

**Status:** design proposal only; no implementation, host decision, or support
claim. The open product decision is whether users need DAW-host capture or
whether offline analysis of an exported file is sufficient. If capture is
needed, name target hosts before selecting VST3, CLAP, or both.

## Job contract

The adapter submits host audio to a format-neutral capture job. A future public
API could expose the following operations and immutable status/result records
without depending on a host SDK:

```cpp
CaptureJob prepare(CaptureConfig, AnalysisSettings);
void submit_audio(const AudioBlockView&) noexcept; // bounded callback operation
void start_capture(HostPosition start);
void stop_capture(HostPosition end);
void cancel();
JobStatus status() const;
AnalysisResult take_result();
```

This is illustrative, not a committed C++ signature. Job states are
`unprepared -> ready -> capturing -> queued -> analyzing -> complete`, with
terminal `cancelled`, `incomplete`, and `failed` states. Start, stop, cancel,
status presentation, and result retrieval run off the audio callback. `prepare`
validates settings and reserves fixed callback storage before activation. A
state or configuration change while capturing ends the current capture as
incomplete; it must never silently combine samples from different formats or
sample rates.

## Audio callback and bounded storage

The callback only copies incoming samples and their host frame position into a
preallocated single-producer/single-consumer ring, updates atomic counters, and
copies the input to the matching output. It does not allocate, lock, wait,
write files, log, display UI, resample, or run analysis. Worker threads drain
the ring to a uniquely created temporary PCM spool and run analysis after
capture stops. Use bounded queues and disk quotas; never grow a queue to keep up
with a slow worker.

For a first implementation, a proposed upper bound is a two-second ring sized
from the active sample rate and channel count, plus a temporary spool capped at
512 MiB and the existing analysis result cap of 512 MiB. The effective maximum
capture duration is the smallest of the user-selected duration, the spool byte
limit divided by the active PCM rate, and the analysis job's resource limit.
Reject a capture that would exceed the configured limit. These numbers are
proposed design values and need product-owner approval and measured host tests;
they are not current product behavior.

If the ring fills, the callback must not wait or overwrite older samples. It
records the first and last missing host frame ranges and transitions the job
to `incomplete`; the worker finalizes only a partial spool marked incomplete,
and the UI must not present it as a complete capture. If the host does not
provide a reliable monotonically increasing frame position, report dropped
frame counts and the discontinuity boundary without inventing an absolute
time. Analysis is cancelled or withheld for incomplete captures unless the
user explicitly chooses to inspect the partial data; any resulting report
retains the incomplete marker and missing-frame provenance.

The spool is scratch data, not an original source recording. Hash the exact
captured PCM after finalization and record its sample format, channel map,
sample rate, captured frame count, host start/end positions when available,
drop ranges, and capture completeness. Do not claim a source-file hash or
original-file identity when the host has not supplied verifiable provenance.
Remove scratch data on completion, cancellation, failure, and normal shutdown;
document recovery behavior for abnormal termination. Never overwrite or
modify host audio.

## Pass-through behavior

The initial adapter is an audio effect with the same input and output bus
layout. It copies every supported input frame unchanged to the corresponding
output frame whether capture is armed, active, stopped, or unavailable. It
does not monitor, gate, normalize, filter, or otherwise alter sound. If the
host supplies a bus arrangement that cannot be paired transparently, reject
activation or report an explicit host error; do not silently produce silence
or remap channels. Validate pass-through for normal, bypass, transport-stop,
offline-render, reset, and error paths in every claimed host.

## Initial format boundaries

These are proposed first-version limits to validate, not current standalone
limits or a host support promise:

| Property | Proposed initial boundary | Behavior outside boundary |
| --- | --- | --- |
| Sample representation | 32-bit floating point input | Reject activation if unavailable; do not silently convert precision. |
| Sample rate | 8,000 through 192,000 Hz, fixed for one capture | Stop and mark incomplete on a rate change; no resampling. A later capture may use the new rate. |
| Channels | 1 through 8 input channels, each kept independent | Reject unsupported layout; no downmix, duplication, or implicit channel reordering. Record the host's channel labels/order where available. |
| Host block size | Any positive block that fits the preallocated ring writer's bounded chunking; zero-frame blocks are no-ops | If a block cannot be consumed within the preallocated callback budget, mark capture incomplete and record its frame range. Never allocate a larger buffer in the callback. |
| Duration/data size | User-selected bound, with the proposed 512 MiB spool ceiling | Stop before exceeding the ceiling and expose the exact captured frame count and reason. |

There is no implicit silence insertion, sample-rate conversion, channel
alignment, or downmix. Each captured channel is analyzed independently. A host
seek or loop creates a discontinuity: stop and mark the capture incomplete,
then require an explicit new capture. The adapter records transport/frame
positions when available, but does not treat transport time as wall-clock time
or evidence of when a recording was made.

## Host lifecycle and threading

Create and destroy instances and UI resources on the host-required control or
main thread. Allocate callback storage and prepare the worker/job before
activation. The host audio thread owns `submit_audio`; one instance is never
processed concurrently from multiple audio threads. Deactivation stops new
callback submissions, signals the worker off-thread, and waits only where the
format/host lifecycle explicitly permits it. Do not join workers or wait for
disk operations from the audio callback. Reset, suspend, resume, bus changes,
sample-rate changes, and destruction must either complete the current job or
mark it incomplete with a reason and release storage safely.

The UI reports capture state, elapsed/captured duration, channel and rate,
resource limit, dropped audio, incomplete reason, analysis progress,
cancellation, and result availability. It must not imply data is complete
while the state is `incomplete`. Saving settings may restore analysis options;
it must not silently resume or synthesize a prior audio capture.

## Decisions required before implementation

1. Is the desired workflow host capture, or file export followed by the
   existing standalone workflow?
2. If host capture is needed, which named host applications and versions matter?
   Validate their plug-in and offline-render behavior before choosing VST3,
   CLAP, or both.
3. Approve capture duration/storage limits, channel/rate bounds, and whether a
   user may analyze partial audio after an overrun.
4. Define report fields and privacy behavior for host names, transport
   positions, temporary audio, and captured-PCM hashes.

Until these decisions and validation gates are complete, this interface is a
bounded proposal only. The shipped product remains a standalone CLI and Qt
desktop application.
