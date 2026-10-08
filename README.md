# HumTrace

HumTrace is a Circuit Drift Labs audio interference analysis project. The repository contains an offline C++ prototype and research/specification set.

## Current prototype

- Reads RIFF/WAVE PCM integer audio (8/16/24/32-bit) and IEEE float audio (32/64-bit).
- Reports sample rate, channels, bit depth, duration, and FFT resolution.
- Computes a Hann-windowed, one-sided spectrum per channel in overlapping frames and reports each frame's strongest non-DC component with its start time.
- Reports nominal 50/60 Hz harmonic-bin measurements with absolute level and local-prominence thresholds.
- Exports versioned JSON measurements while keeping observed energy separate from source attribution.

This is an engineering prototype, not beta v0.1. The CLI streams WAV audio through bounded rolling analysis windows, but retains the generated timeline and JSON report in memory; `decode_wav` remains an in-memory convenience wrapper. The timeline reports each frame independently; it does not associate peaks into persistent tracks or compare recordings. JSON output is a measurement export, not a forensic chain-of-custody report. There is no desktop GUI or plugin yet.

## Build and test

Requires a C++20 compiler and `make`.

```sh
make
make test
build/humtrace recording.wav
build/humtrace recording.wav --json analysis.json
```

## Design notes

- [DSP algorithm research](docs/research/dsp-algorithms.md)
- [Architecture and module contracts](docs/architecture/overview.md)
- [Requirements and staged delivery](docs/specs/product-requirements.md)
- [JSON report schema](docs/modules/report-schema-v1.md)
- [Known limitations](docs/user-guide/limitations.md)

## Measurement interpretation

Levels are peak-amplitude dBFS calculated from normalized decoded samples. They are not SPL or voltage measurements. Similarity to 50/60 Hz harmonic patterns does not prove a ground loop or identify a device, location, or recording origin.
