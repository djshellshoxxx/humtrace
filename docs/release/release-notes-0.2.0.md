# HumTrace 0.2.0 preview release notes

Pre-release. Unsigned binaries; validated on synthetic signals only (see [limitations](../user-guide/limitations.md)).

## New in 0.2.0
- **Welch PSD:** shared streaming estimator, median density floor, nominal 50/60 Hz harmonic prominence, CLI segment/hop controls, JSON export, desktop summaries. Density units are dBFS^2/Hz and separate from peak-amplitude dBFS.
- **Raw frame levels:** signed mean/DC, RMS, and sample peak before detrending/windowing, exported with explicit numeric-floor flags and displayed in the finding inspector. RMS includes DC; sample peak is not intersample true peak.
- **Musical labels:** every reported frequency can be shown as an equal-tempered note with cents (CLI track lines, GUI inspector and frame table, `--a4` tuning reference 400–480 Hz). 60 Hz ≈ B1 −49c, 50 Hz ≈ G1 +35c. Labels are not evidence of musical content or of a source.
- **CSV export:** `humtrace in.wav --csv tracks.csv` writes one row per persistent track (times, median Hz, note, cents, level, drift, observed/missing frames). Existing outputs are never overwritten without `--force`.
- **macOS build** (Apple silicon, ad-hoc signed `.app` + CLI) added to CI and releases.
- Release pipeline: tagged/main/branch builds publish Linux (`.deb`, `.tar.gz`), Windows (`.zip`) and macOS (`.zip`) packages with SHA-256 sums.

## Platform notes
- macOS: the app is not notarized; first launch needs right-click → Open. Intel Macs are not built.
- Windows: SmartScreen will warn (unsigned).
- Linux: Ubuntu 24.04 baseline `.deb`; `.tar.gz` for other distributions with Qt 6.

## Carried over from 0.1.0
See [0.1.0 notes](release-notes-0.1.0-beta.md) for formats, analysis, GUI and known gaps.
