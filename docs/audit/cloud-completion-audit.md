# Cloud completion audit

The main-line prototype was missing substantial implementation already present in
`claude/project-completion-builds-sokivz`. That branch is integrated here with its
history preserved. Its earlier QA statements describe earlier runs, not evidence
that this merge or a new platform package has passed.

## Implemented development workflow

The dependency-free C++20 core and CLI include streaming WAV decoding, per-channel
FFT/peak/harmonic measurements, Welch PSD, raw sample levels, descriptive peak
tracks, independent-recording comparison, SHA-256 input integrity metadata,
versioned JSON and track CSV exports. The Qt desktop includes background jobs,
progress/cancel, waveform/spectrum/spectrogram views, measurement tables and
finding inspection. Native CI builds Linux, Windows, Apple silicon macOS and Intel
macOS separately; release artifacts must correspond to its exact tested commit.

This revision adds export collision rejection, exclusive temporary creation,
no-replace publication without `--force`, Windows replacement support, UTF-8 path
regressions, Qt 6.8-compatible bounded GUI test waits, and optional-view allocation
avoidance for the CLI. Report equality tests protect numerical parity between
presentation-enabled and CLI analysis.

## Uncompleted requirements

- Desktop and comparison workflows still retain capped frame/track results and
  spectrograms in memory; disk-backed presentation and tile eviction remain. The
  standalone `--stream --json` path now writes incrementally with explicit disk
  quotas, free-space checks and bounded active tracking. Its one/three-hour
  acceptance uses a real 96 MiB address-space limit; passing it does not establish
  the desktop constant-working-set requirement.
- Track split/merge lineage, full event intervals, fit-quality/quantile output and
  independently validated persistence rules remain incomplete.
- A labeled recording corpus split by source/session, field performance and
  forensic accuracy claims are unavailable. Synthetic or unlabeled real-file
  checks do not satisfy this gate.
- Packaged Narrator/AT-SPI, high-DPI, keyboard focus, clean-machine install/upgrade
  and uninstall tests require their own platform evidence.
- Full report provenance/import/verification workflow, dependency SBOM and exact
  redistributed Qt license/compliance review remain outstanding.
- ENF matching, signature libraries, hardware diagnostic guidance and plugin
  adapters are separately deferred specs. Standalone archives are not plugins.

A native preview release may expose the implemented functionality with these
limitations, but does not close the full beta acceptance checklist. Passing a
memory ratio or a native build alone must not be described as completing these
requirements. See `release/beta-checklist.md` and each detailed specification.
