# Long-file analysis, progress, cancellation, and memory specification

## User-visible contract

Opening a file is read-only. Metadata appears before full analysis where the decoder permits it. Analysis runs on a worker; the window remains interactive. Progress states the current stage, channel, and completed/total frames when known. Cancel is available during hash, decode, DSP, and report generation; cancellation returns promptly at a bounded checkpoint and produces a visibly partial/cancelled result.

## Worker model

- One job owns its decoder, settings snapshot, cancellation token, progress snapshot, and bounded scratch buffers.
- UI submits a job and receives immutable value snapshots or queued events. The worker never reads widget state and never calls UI APIs directly.
- Cancellation token is checked between bounded read batches, channels, and analysis frames. Set a target cancellation latency (default <=250 ms on reference machine for decode/DSP stage) and measure it.
- Job state transitions are `queued -> hashing -> decoding -> analyzing -> finalizing -> complete`; any active stage may move to `cancelling -> cancelled`, or `failed`. Invalid transitions are rejected and tested.
- Closing the window asks active jobs to cancel and joins workers before destroying referenced data. No detached worker threads.

## Bounded memory

Audio working memory is bounded by `channels * (analysis_window + decoder_batch + overlap) * sizeof(sample)` plus FFT work buffers and fixed metadata. UI caches use multiresolution waveform summaries and spectrogram tiles with a configured byte budget and eviction. Reports are written incrementally to a temporary JSON file; they are not retained as an unbounded tree of all frames. The in-memory API may remain for small-file tests but must enforce a caller-visible cap.

Project targets: configurable resident memory cap, default 512 MiB for desktop worker + caches on supported systems; temporary disk cap and free-space checks before report/tile creation. Do not silently downsample or discard channels to meet limits. On resource exhaustion, stop safely and identify the limit/stage.

## Progress semantics

Progress is monotonic within a stage. If total work is unknown, show indeterminate progress and a useful activity label rather than a false percentage. Stages have weights only after benchmark data exists; otherwise display stage and counts. Progress frequency is throttled (e.g. <=10 UI updates/sec) and cannot dominate DSP runtime.

## Verification

Use a generated long WAV and constrained memory limit to measure working set; verify work scales with block/window/cache settings rather than input duration, excluding report output on disk. Test cancel at every state, close while active, corrupted input, slow storage, insufficient disk, and repeated start/cancel cycles under sanitizers. Record cancellation latency and ensure no source modification or orphan temp file.
