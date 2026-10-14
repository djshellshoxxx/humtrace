# HumTrace 0.1.0 beta release notes

## Included
- Qt 6 desktop app (`humtrace-gui`) and CLI (`humtrace`).
- WAV decoding: PCM 8/16/24/32, float 32/64, extensible; streaming, bounded window memory, SHA-256 of input.
- Per-channel spectrum, 50/60 Hz harmonic measurements, prominence-gated peaks, persistence tracks.
- Waveform, spectrogram, spectrum + table, findings, inspector; keyboard operation; high-contrast theme.
- Cancellable analysis; atomic JSON export (schema 1.2: input hash and size, all result-affecting settings, application/method versions, warnings). Paths are reduced to the file name by default in the CLI and GUI; two-recording comparison.

## Not included / not validated
- No AIFF/FLAC/MP3, RF64, resampling, alignment, cross-spectral comparison.
- Track persistence is descriptive; no calibrated confidence, no source attribution.
- Validation is synthetic only ([results](../validation/synthetic-results.md)); no labeled real-recording corpus yet.
- Windows build and packages, screen-reader checks (Narrator / AT-SPI), and clean-VM install tests have not been run by the project; see the [beta checklist](beta-checklist.md).

## Packages
Linux: `.deb` and `.tar.gz` (Ubuntu 24.04 baseline). Windows: `.zip` from CI (unverified). SHA-256 sums are attached to each Linux CI run.
