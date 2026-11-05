# Developer Overview

## What it is

A small C++20 program with three library modules and one CLI. It has no runtime dependencies beyond the standard library. Strict compiler flags are on (`-Wall -Wextra -Wpedantic -Wconversion -Werror`).

## Design decisions (and why)

**Standard library only.** Only the C++ standard library is included. The FFT is hand written (`fft()` in `src/spectrum.cpp`), so the code stays easy to audit.

**Three separate concerns.** `wav` reads files, `spectrum` does the maths, and `report` writes JSON. Each has its own header under `include/humtrace/`. The CLI in `src/main.cpp` only wires them together. This matches the planned module split in `docs/architecture/overview.md` (`dsp`, `analysis`, and so on).

**Streaming read, in-memory result.** `WavStreamReader::read_frames()` reads bounded blocks, so the input is not loaded whole. The timeline and JSON stay in memory, so long recordings still grow memory. This is a known prototype limit, and `decode_wav` loads everything at once.

**Power-of-two frames.** `analyze_spectrum` requires a power-of-two sample count. `main.cpp` picks the largest power of two that fits within the smaller of the file length and the sample rate, capped at 65,536, with a 50 percent hop. Files shorter than 4 samples are refused.

**Observed versus attributed.** Each measurement is stored as observed energy with its levels and thresholds. Attribution to a source is left out on purpose. The docs and reports say this in plain language.

**Versioned JSON.** `serialize_report_json()` writes `schema_version` "1.0" (`src/report.cpp`), matching `schema/analysis-report-v1.schema.json`. Changes to fields need a new version.

**Honest levels.** Levels are peak dBFS on normalised samples. The code does not convert to SPL or voltage.

## Pipeline

```
recording.wav
   │
   ▼
WavStreamReader (wav.cpp)           bounded blocks, validates format
   │  samples (double), sample_rate, channels, bit depth
   ▼
analyze_tone_timeline()  ──► per-frame strongest bin          (spectrum.cpp)
analyze_interference_timeline() ──► per-frame 50/60 Hz checks  (spectrum.cpp)
   │   uses analyze_spectrum() (Hann window, FFT),
   │        strongest_peak(), measure_harmonics()
   ▼
AnalysisReport (report.hpp)
   │
   ▼
serialize_report_json()  ──► --json file   (report.cpp)
   │
   └──► summary printed by main.cpp
```

## Threads and state

Single thread. There is no shared state between runs. The FFT and analysis functions take const references and return new vectors.

## Data

- `WavMetadata`, `WavAudio` (`wav.hpp`): format and decoded samples
- `SpectrumBin`, `SpectralPeak`, `ToneFrame` (`spectrum.hpp`): per-bin and per-frame results
- `HarmonicMeasurement`, `HarmonicCandidate`, `InterferenceFrame` (`spectrum.hpp`): mains checks
- `ReportChannel`, `AnalysisReport` (`report.hpp`): the JSON model

See [[Developer-Code-Map]] for locations and [[Developer-Conventions]] for the rules.
