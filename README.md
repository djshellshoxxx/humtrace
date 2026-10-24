# HumTrace

HumTrace is a Circuit Drift Labs software artwork and audio interference analysis project. The repository contains an offline C++ application and research/specification set.

## What it does

- Reads WAV (PCM 8/16/24/32, float 32/64, extensible) in bounded streaming windows; records the input SHA-256.
- Computes streaming Welch PSD, median noise density, and nominal harmonic prominence in dBFS^2/Hz.
- Reports raw per-frame mean/DC, RMS, and sample peak with explicit numeric-floor flags.
- Measures per-channel spectra, strongest peaks, prominence-gated peaks, and 50/60 Hz harmonic bins; groups peaks into descriptive persistence tracks (gaps counted, never interpolated).
- Desktop app (Qt 6): waveform, spectrogram, spectrum with table alternative, findings list and inspector, keyboard operation, high-contrast theme, cancellable analysis, atomic JSON export, two-recording comparison.
- CLI: `humtrace file.wav --json report.json`; `humtrace compare a.wav b.wav`.

This is a development build: synthetic validation does not establish forensic accuracy, source attribution, or chain of custody. Desktop and native package readiness must be established by the corresponding tests and release gates. See the [user guide](docs/user-guide/user-guide.md), [limitations](docs/user-guide/limitations.md), and [release notes](docs/release/release-notes-0.2.5.md).

## Build and test

Core + CLI (C++20 compiler and `make`): `make && make test` (`make fuzz` runs the 30 s settings fuzz).
Everything incl. the Qt 6 app and packages (CMake 3.22+, Qt 6.4+): `cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-cmake && (cd build-cmake && ctest && cpack)`.

## Design notes

- [Complete design and build index](docs/design/index.md)
- [DSP algorithm research](docs/research/dsp-algorithms.md)
- [GUI toolkit decision research](docs/research/gui-toolkit-decision-research.md)
- [Architecture and module contracts](docs/architecture/overview.md)
- [Requirements and staged delivery](docs/specs/product-requirements.md)
- [Desktop GUI and visual design](docs/specs/gui.md)
- [JSON report schema](docs/modules/report-schema-v1.md)
- [QA results](docs/testing/qa-report.md)
- [Known limitations](docs/user-guide/limitations.md)

## Measurement interpretation

Spectrum levels are peak-amplitude dBFS calculated from normalized decoded samples. Welch density uses dBFS^2/Hz, a separate power-per-hertz scale. They are not SPL or voltage measurements. Similarity to 50/60 Hz harmonic patterns does not prove a ground loop or identify a device, location, or recording origin.

## Musical labels and CSV

`humtrace in.wav --csv tracks.csv --a4 440` adds nearest-note names and cents (e.g. 60 Hz ≈ B1 −49c) to console output and a per-track CSV. See the [feature note](docs/specs/musical-features.md). The native CI workflow builds Linux, Windows and macOS (Apple silicon and Intel) packages; only successfully validated and published runs establish availability in GitHub [releases](../../releases).

## Welch PSD

`humtrace file.wav --psd-segment-size 4096 --psd-hop-size 2048 --json report.json --quiet`

Both flags also apply to `compare`. The desktop uses the same estimator with default settings. See [PSD units and limitations](docs/modules/welch-psd.md).

## Bounded-memory long-file JSON reports

`humtrace file.wav --stream --json report.json --memory-limit-mib 64 --disk-limit-mib 1024`

The explicit streaming workflow spools measurements and track observations beside
its destination, computes exact track medians using bounded disk scans, and
publishes a schema-compatible report atomically. It checks available disk space,
limits temporary plus final output bytes, and removes scratch on cancellation or
failure. Disk I/O trades speed for bounded memory. Combined JSON/CSV exports use the capped retained workflow; ordinary long/dense
CLI analysis and comparison summaries page automatically. `--stream`
accepts one recording and JSON output. The desktop uses disk-backed measurements, bounded caches, multiresolution
spectrogram tiles and paged tables for long/dense runs.

`python tools/validate_streaming_memory.py build/humtrace` is the Linux acceptance
runner: persistent-tone recordings of one and three hours complete under a real
96 MiB address-space limit and a 64 MiB configured scratch budget, with source
hash preservation, full frame counts and cleanup checks. This does not establish
field accuracy or complete the desktop long-file specification.

## Configurable measurements and tracking

Frame/hop, spectrum/Welch detrending, peak band/separation, local baseline, thresholds
and nominal-harmonic search tolerance are explicit in the desktop and CLI. Reports
use schema 1.3 and preserve effective settings, raw/refined peak and resolution
metadata, quartiles, drift fit quality, gaps and candidate ambiguity events. Resolution
is distinct from calibrated uncertainty, which remains uncharacterized. See the
[user guide](docs/user-guide/user-guide.md) and [acceptance ledger](docs/release/acceptance-0.2.4.md).

VST3/CLAP adapters require a host/capture workflow decision and separate SDK/host
validation. The [capture interface proposal](docs/specs/plugin-capture-interface.md)
defines that boundary; standalone archives are not plugin binaries.
