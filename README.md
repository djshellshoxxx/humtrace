# HumTrace

HumTrace is a Circuit Drift Labs audio interference analysis project. The repository contains an offline C++ prototype and research/specification set.

## Current prototype

- Reads RIFF/WAVE PCM integer audio (8/16/24/32-bit) and IEEE float audio (32/64-bit).
- Reports sample rate, channels, bit depth, duration, and FFT resolution.
- Computes a Hann-windowed, one-sided spectrum per channel in overlapping frames and reports each frame's strongest non-DC component with its start time.
- Reports nominal 50/60 Hz harmonic-bin measurements with absolute level and local-prominence thresholds.
- Associates local spectral peaks across frames into persistent per-channel tracks (explicit gaps, drift estimate) and labels each frequency with its nearest equal-tempered note and cents offset (`--a4` sets the reference pitch).
- Records the input SHA-256 and byte size, detects a file changing during analysis, and writes JSON reports atomically (basename-only path unless `--full-path`).
- Compares track summaries of two recordings (`--compare ref.wav`) without alignment or resampling; unmatched tracks are listed, never scored as zero.
- Supports progress output (`--progress`) and cancellation (Ctrl-C leaves no report).
- Exports versioned JSON (schema 1.1) measurements while keeping observed energy separate from source attribution.

This is an engineering prototype, not beta v0.1. Not yet implemented: GUI, waveform/spectrogram views, Welch PSD, non-WAV formats, ENF, signature library, plugin adapter, and a labeled validation corpus. The CLI streams WAV audio through bounded rolling analysis windows, but retains the generated timeline and JSON report in memory; `decode_wav` remains an in-memory convenience wrapper. Track association and comparison are uncalibrated descriptive measurements. A SHA-256 digest identifies a byte sequence only. JSON output is a measurement export, not a forensic chain-of-custody report. There is no desktop GUI or plugin yet. The build instructions below apply to the command-line prototype, not to the proposed GUI or plugin.

## Build and test

Requires a C++20 compiler and `make`.

```sh
make
make test
build/humtrace recording.wav
build/humtrace recording.wav --json analysis.json
build/humtrace recording.wav --compare other.wav --compare-tolerance 1.0 --progress
```

## Design notes

- [Complete design and build index](docs/design/index.md)
- [DSP algorithm research](docs/research/dsp-algorithms.md)
- [GUI toolkit decision research](docs/research/gui-toolkit-decision-research.md)
- [Architecture and module contracts](docs/architecture/overview.md)
- [Requirements and staged delivery](docs/specs/product-requirements.md)
- [Desktop GUI and visual design](docs/specs/gui.md)
- [JSON report schema](docs/modules/report-schema-v1.md)
- [Known limitations](docs/user-guide/limitations.md)

## Measurement interpretation

Levels are peak-amplitude dBFS calculated from normalized decoded samples. They are not SPL or voltage measurements. Similarity to 50/60 Hz harmonic patterns does not prove a ground loop or identify a device, location, or recording origin.
