# HumTrace

HumTrace is a Circuit Drift Labs audio interference analysis project. The repository contains an offline C++ prototype and research/specification set.

## Current prototype

- Reads RIFF/WAVE PCM integer audio (8/16/24/32-bit) and IEEE float audio (32/64-bit).
- Reports sample rate, channels, bit depth, duration, and FFT resolution.
- Computes a Hann-windowed, one-sided spectrum per channel in overlapping frames and reports each frame's strongest non-DC component with its start time.
- Reports nominal 50/60 Hz harmonic-bin measurements with absolute level and local-prominence thresholds.
- Associates per-frame spectral peaks into persistent tone tracks (frequency, drift, level, persistence, proximity to n x 50/60 Hz) and compares tracked tones across files (`--compare`, optional JSON).
- Exports versioned JSON measurements while keeping observed energy separate from source attribution.

This is a **beta (v0.1.0-beta.1)** with a command-line tool and a Qt desktop viewer (waveform, log-frequency spectrogram with track overlay, spectrum, tracks table, comparison dialog with JSON export). The GUI has been built and smoke-tested on Linux only; the Windows build is defined in CI but has not been run, and no part of the GUI has been visually reviewed by a human or tested with a screen reader. There is no validated real-recording corpus, so detector false-positive/false-negative rates are unknown. The CLI streams WAV audio through bounded rolling windows but retains the timeline, tracks and JSON report in memory. Tone tracking is greedy nearest-frequency association with fixed limits (see [limitations](docs/user-guide/limitations.md)). JSON output is a measurement export, not a forensic chain-of-custody report. There is no plugin. See [SPEC_STATUS.md](SPEC_STATUS.md) for exactly what is and is not done.

## Build and test

Requires a C++20 compiler and `make`.

```sh
make && make test                       # CLI only, no dependencies
build/humtrace recording.wav --json analysis.json [--frame-size 8192 --threshold -60 --prominence 10]
build/humtrace --compare a.wav b.wav [--json comparison.json]
# CLI + Qt 6 GUI + ctest (needs cmake, ninja, qt6-base-dev):
cmake -S . -B build-cmake -G Ninja && cmake --build build-cmake && ctest --test-dir build-cmake
```

Reports never overwrite the input; an existing report is kept as `.bak`. Ctrl-C cancels.

## Design notes

- [Complete design and build index](docs/design/index.md)
- [DSP algorithm research](docs/research/dsp-algorithms.md)
- [GUI toolkit decision research](docs/research/gui-toolkit-decision-research.md)
- [Architecture and module contracts](docs/architecture/overview.md)
- [Requirements and staged delivery](docs/specs/product-requirements.md)
- [Desktop GUI and visual design](docs/specs/gui.md)
- [JSON report and comparison schemas](docs/modules/report-schema-v1.md)
- [Known limitations](docs/user-guide/limitations.md)

## Measurement interpretation

Levels are peak-amplitude dBFS calculated from normalized decoded samples. They are not SPL or voltage measurements. Similarity to 50/60 Hz harmonic patterns, or a tone track lying near a multiple of 50/60 Hz, does not prove a ground loop or identify a device, location, or recording origin.
