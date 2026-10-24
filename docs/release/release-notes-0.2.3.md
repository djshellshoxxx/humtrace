# HumTrace 0.2.3 development preview

Long desktop analyses now retain their complete measurements in private disk
storage instead of a duration-sized result tree. The inspector reads exact frame
and track values through a 6 MiB LRU. Spectrogram rendering uses a disk pyramid
and a viewport image limited to 2 MiB; its visible label states the aggregation
factor, and zoom reveals original frames. Findings and comparison tables page
256 rows at a time without dropping underlying measurements.

JSON export runs in a cancellable background job and writes incrementally to an
atomic temporary destination. Storage has explicit disk/free-space/working-buffer
limits. Recording comparison loads bounded summaries, checks candidate budgets
and supports cancellation; an oversized workload reports its limit explicitly.

Parity tests compare disk-backed frames, inspector text, report JSON and
comparison results against the capped in-memory API. The Linux GUI acceptance
analyzes complete one/three-hour recordings, samples cursor/viewport positions,
checks cache budgets and event-loop heartbeat, and measures process peak RSS.
Measured peak RSS was 35.66 MiB for one hour and 36.00 MiB for three hours
(0.34 MiB growth); both completed with thousands of GUI heartbeat callbacks.
These synthetic implementation checks establish no field accuracy claim.

Track event/lineage details, full provenance/import, validation-corpus and native
accessibility evidence remain open; see the completion audit. Packages are
standalone applications, without VST3/CLAP adapters. Windows builds are unsigned;
macOS builds are ad-hoc signed and not notarized. This preview does not complete
all specifications or establish forensic source identification.
