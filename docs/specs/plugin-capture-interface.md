# Plugin capture and analysis interface

**Status:** chosen implementation contract; not implemented or host-validated.
The first workflow captures a bounded live DAW input through a transparent
pass-through effect, then runs the existing offline analysis after capture
stops. VST3 and CLAP are selected for the first plugin scope. REAPER 7.83 is
the first manual host target on Linux x86-64, Windows x86-64, macOS ARM64, and
macOS Intel. These choices do not establish support before the listed tests
pass.

## Selected defaults

- Accept mono or stereo IEEE 32-bit float audio at a fixed 8 kHz to 192 kHz
  rate per activation. Reject other channel layouts, representations, and
  rates; do not downmix or resample.
- Default capture length is 60 seconds. The user can select 1 to 120 seconds.
- Allocate a two-second single-producer/single-consumer ring and negotiate a
  host maximum block size that fits it. Reject activation when it does not fit.
- Cap the temporary IEEE-float WAV spool at 512 MiB including its header,
  disk-backed analysis at 512 MiB, report export at 128 MiB, and aggregate
  capture/analysis/export storage at 1 GiB. A legal 120-second stereo capture
  stays below the aggregate limit.
- Start asynchronously. The first accepted positive-length audio block is the
  capture start; automation is not sample-accurate. Stop closes admission to
  new blocks and drains accepted samples even if the host makes no later audio
  callback.
- Overrun preserves the accepted prefix and records the first missing interval
  as incomplete. Incomplete, cancelled, reset, and format-change captures are
  not analyzed or automatically exported. The user must start a new capture.
- Save a report only after complete analysis and only on explicit command.
  Create a unique output without replacing an existing report. Settings may be
  restored; an active job, path, or audio is never restored or auto-started.
- Use `%LOCALAPPDATA%/HumTrace/Reports` on Windows,
  `~/Library/Application Support/HumTrace/Reports` on macOS, and
  `$XDG_DATA_HOME/humtrace/reports` on Linux, defaulting to
  `~/.local/share/humtrace/reports`. Show the resulting path after a successful
  save.

The plugin records the interleaved captured PCM SHA-256 separately from the
generated WAV byte hash. Capture UTC values are software clock observations.
Host name, project, track, and path are not included. Optional host sample
positions describe DAW transport only; they are not wall-clock or source
recording timestamps. The official [REAPER download page](https://www.reaper.fm/download.php)
lists version 7.83 on 2026-10-24, and its [format list](https://www.reaper.fm/about.php)
includes VST3 and CLAP. That establishes format applicability, not HumTrace
compatibility.

Capture reports add a `capture` section with `adapter_format`, `pcm_sha256`,
`captured_frames`, `limit_frames`, `sample_rate_hz`, `channels`, `stop_reason`,
optional `host_start_sample` and `host_end_sample_exclusive`, and
`requested_utc`/`completed_utc`. Each field describes captured-signal
provenance and does not assert source-file identity.

## Job contract

Each adapter submits host audio to a format-neutral capture job. The host SDK
stays outside the core API:

```cpp
enum class CapturePhase { Ready, Preparing, Armed, Capturing, Draining,
                          Analyzing, Complete, Incomplete, Cancelled, Failed };
enum class RequestResult { Accepted, Busy, NoResult, AlreadySaved, InvalidArgument };
RequestResult request_capture(uint32_t seconds) noexcept;
RequestResult request_stop() noexcept;
RequestResult request_cancel() noexcept;
RequestResult request_save() noexcept;
void submit_audio(const PlanarF32Block&) noexcept;
CaptureStatus status() const noexcept;
std::shared_ptr<const CaptureSummary> result_summary() const; // control thread
```

The signature is a selected API target, not implemented code. Construct the
job and start its persistent worker before activation. The audio callback may
submit blocks and issue atomic capture/stop/cancel commands only. State display,
save, and all ownership changes run on the control thread. The outer endpoint,
worker, and ring remain alive until the host detaches the audio endpoint and
off-thread teardown joins the worker. A format or configuration change ends
the current capture as incomplete; samples from different formats or rates
are never combined.

## Audio callback and bounded storage

The callback copies incoming samples into a preallocated
single-producer/single-consumer ring, updates atomic counters, and copies input
to the matching output. It does not allocate, lock, wait, make system wakeup
calls, write files, log, display UI, resample, or run analysis. A persistent
worker drains the ring to a uniquely created temporary WAV and runs analysis
after stop. Use bounded storage; never grow a queue to keep up with a slow
worker.

Size all fixed buffers from the negotiated rate, channel count, and host maximum
block size before activation. Check quotas before capture and before export.
The worker writes IEEE-float WAV, hashes the captured PCM, closes the spool,
then invokes existing paged file analysis. After cleanup the job retains only
an immutable summary and report provenance, not the analysis timeline or PCM.

If the ring fills or the spool quota would be exceeded, the callback does not
wait or overwrite older samples. Stop accepting whole new blocks, preserve the
accepted prefix, record the first missing interval, and mark the job incomplete.
The adapter continues unchanged pass-through while the capture job reports the
overrun. Do not analyze or export an incomplete capture. If the host lacks
reliable sample positions, report dropped-frame counts and the discontinuity
boundary without inventing absolute time.

The spool is scratch data, not an original source recording. Record its sample
format, channel order, rate, captured frame count, stop reason, optional host
sample positions, first missing interval, and completeness. Remove scratch data
on completion, cancellation, failure, and normal shutdown; leftovers after a
crash are never auto-imported. Never overwrite or modify host audio.

## Pass-through behavior

The initial adapter is an audio effect with the same input and output bus
layout. It copies every supported input frame unchanged to the corresponding
output frame whether capture is armed, active, stopped, or unavailable. It
does not monitor, gate, normalize, filter, or otherwise alter sound. If the
host supplies a bus arrangement that cannot be paired transparently, reject
activation or report an explicit host error; do not silently produce silence
or remap channels. Validate pass-through for normal, bypass, transport-stop,
offline-render, reset, and error paths in every claimed host.

## Capture boundaries

These are the selected plugin limits. They are narrower than the standalone
reader and are not a host-support promise:

| Property | Selected boundary | Behavior outside boundary |
| --- | --- | --- |
| Sample representation | 32-bit floating point input | Reject activation if unavailable; do not silently convert precision. |
| Sample rate | 8,000 through 192,000 Hz, fixed for one activation | Reject unsupported rate; a mid-capture change marks the capture incomplete. No resampling. |
| Channels | Mono or stereo, each kept independent | Reject other layouts. No downmix, duplication, or implicit channel reordering. |
| Host block size | Negotiated maximum must fit the preallocated two-second ring; zero-frame blocks are no-ops | Reject activation when the maximum cannot fit. Never resize buffers from the audio callback. |
| Duration/data size | 60 seconds by default, user-selected 1 to 120 seconds; 512 MiB spool ceiling | Stop at the exact limit, preserve accepted samples as incomplete, and expose the stop reason. |

There is no implicit silence insertion, sample-rate conversion, channel
alignment, or downmix. Each captured channel is analyzed independently. A host
seek or loop creates a discontinuity: stop and mark the capture incomplete,
then require an explicit new capture. The adapter records transport/frame
positions when available, but does not treat transport time as wall-clock time
or evidence of when a recording was made.

## Host lifecycle and threading

Create and destroy instances and UI resources on the host-required control or
main thread. Allocate callback storage and start the persistent worker before
activation. The host audio thread owns `submit_audio`; one instance is never
processed concurrently from multiple audio threads. Deactivation closes the
audio endpoint and marks an active capture incomplete; join and cleanup happen
off the audio thread. Reset, suspend, resume, bus changes, sample-rate changes,
and destruction must either complete the current job or mark it incomplete
with a reason. Format-specific callbacks must honor their own thread rules.

The UI reports capture state, elapsed/captured duration, channel and rate,
resource limit, dropped audio, incomplete reason, analysis progress,
cancellation, and result availability. It must not imply data is complete
while the state is `incomplete`. Saving settings may restore analysis options;
it must not silently resume or synthesize a prior audio capture.

## Acceptance still required

1. Implement and test the reusable bounded capture job, capture provenance
   fields, atomic report export, and scratch cleanup.
2. Build VST3 and CLAP adapters with the pinned SDKs and official validators.
   Test lifecycle, repeated instances, real-time behavior, pass-through,
   capture parity, cancellation, quotas, and state handling on every native
   platform/architecture baseline.
3. Manually validate both formats in the current REAPER 7.83 release on those
   baselines using an existing host license. The official [download page](https://www.reaper.fm/download.php)
   lists its current 60-day evaluation; this work does not acquire a license or
   accept new terms.
4. Keep provenance explicit: a captured-PCM hash is not an input-file hash;
   host transport position is not recording time; neither proves authenticity
   or source identity.

Until implementation and validation pass, all published products remain the
standalone CLI and Qt desktop application.
