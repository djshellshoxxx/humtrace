# HumTrace

HumTrace is a Circuit Drift Labs audio interference analysis project. The repository contains an offline C++ prototype and research/specification set.

## What it does

- Reads WAV (PCM 8/16/24/32, float 32/64, extensible) in bounded streaming windows; records the input SHA-256.
- Computes streaming Welch PSD, median noise density, and nominal harmonic prominence in dBFS^2/Hz.
- Reports raw per-frame mean/DC, RMS, and sample peak with explicit numeric-floor flags.
- Measures per-channel spectra, strongest peaks, prominence-gated peaks, and 50/60 Hz harmonic bins; groups peaks into descriptive persistence tracks (gaps counted, never interpolated).
- Desktop app (Qt 6): waveform, spectrogram, spectrum with table alternative, findings list and inspector, keyboard operation, high-contrast theme, cancellable analysis, atomic JSON export, two-recording comparison.
- CLI: `humtrace file.wav --json report.json`; `humtrace compare a.wav b.wav`.

This is a **beta**: validated on synthetic signals only, no source attribution, no chain-of-custody claims. See the [user guide](docs/user-guide/user-guide.md), [limitations](docs/user-guide/limitations.md), and [release notes](docs/release/release-notes-0.1.0-beta.md).

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

`humtrace in.wav --csv tracks.csv --a4 440` adds nearest-note names and cents (e.g. 60 Hz ≈ B1 −49c) to console output and a per-track CSV. See the [feature note](docs/specs/musical-features.md). Prebuilt Linux, Windows and macOS (Apple silicon) packages are attached to GitHub [releases](../../releases).

## Welch PSD

`humtrace file.wav --psd-segment-size 4096 --psd-hop-size 2048 --json report.json --quiet`

Both flags also apply to `compare`. The desktop uses the same estimator with default settings. See [PSD units and limitations](docs/modules/welch-psd.md).
