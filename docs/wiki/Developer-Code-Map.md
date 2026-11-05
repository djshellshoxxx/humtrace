# Developer Code Map

Paths are relative to the repo root.

## Directory tree

```
humtrace/
├── Makefile                      build and test targets (C++20, strict warnings, -Werror)
├── include/humtrace/
│   ├── wav.hpp                   WavMetadata, WavAudio, WavStreamReader (public WAV API)
│   ├── spectrum.hpp              SpectrumBin, SpectralPeak, HarmonicMeasurement, HarmonicCandidate, ToneFrame, InterferenceFrame
│   └── report.hpp                ReportChannel, AnalysisReport
├── src/
│   ├── wav.cpp                   RIFF/WAVE parser, PCM and float decoding, streaming reads
│   ├── spectrum.cpp              fft(), analyze_spectrum(), strongest_peak(), measure_harmonics(),
│   │                             analyze_tone_timeline(), analyze_interference_timeline()
│   ├── report.cpp                serialize_report_json() and JSON helpers
│   └── main.cpp                  CLI: argument check, frame sizing, analysis, summary, --json
├── tests/
│   ├── test_wav.cpp              WAV parsing and decoding
│   ├── test_spectrum.cpp         FFT and harmonic analysis
│   ├── test_report.cpp           JSON output
│   └── test_cli.py               end-to-end CLI test (Python)
├── schema/analysis-report-v1.schema.json   JSON Schema for the report
├── docs/
│   ├── architecture/             overview.md (module plan), decisions.md
│   ├── design/index.md           complete design index
│   ├── research/                 DSP algorithms, GUI toolkit decision
│   ├── specs/                    product requirements, GUI spec
│   ├── modules/                  report-schema-v1.md
│   ├── user-guide/limitations.md
│   ├── testing/, performance/, release/, integration/, troubleshooting/, specs/
│   └── wiki/                     these wiki pages
├── .github/workflows/ci.yml      Ubuntu (make, make test) and Windows (MSYS2, make)
├── shared/, skills/              shared Circuit Drift Labs tooling and bundled skills (not app code)
└── README.md
```

## Key symbols

| Symbol (file:approx line) | Purpose |
|---|---|
| `class WavStreamReader` (`include/humtrace/wav.hpp:29`) | streaming reader. `metadata()`, `read_frames(max_frames)` |
| `WavMetadata` (`wav.hpp:21`), `WavAudio` (`wav.hpp:10`) | format and decoded data |
| `decode_wav` (declared in `wav.hpp`) | in-memory convenience read. Keeps every sample |
| `fft()` (`src/spectrum.cpp:12`) | in-place complex FFT |
| `analyze_spectrum()` (`src/spectrum.cpp:70`) | Hann-windowed one-sided spectrum. Needs a power-of-two length |
| `strongest_peak()` (`src/spectrum.cpp:102`) | strongest non-DC bin |
| `measure_harmonics()` (`src/spectrum.cpp:126`) | nominal 50 or 60 Hz harmonics. Default threshold -60 dBFS, up to 10 harmonics (`spectrum.hpp:71`) |
| `analyze_tone_timeline()` (`src/spectrum.cpp:163`) | per-frame strongest bin with start time |
| `analyze_interference_timeline()` (`src/spectrum.cpp:185`) | per-frame mains checks |
| `serialize_report_json()` (`src/report.cpp:62`) | writes `schema_version` 1.0 |
| `main()` (`src/main.cpp:14`) | usage check, frame sizing (lines 26 to 33), output |

## Where do I find X / to change X, edit Y

| I want to... | Edit |
|---|---|
| support another WAV format (for example 24-bit float or WAVE_FORMAT_EXTENSIBLE variants) | `src/wav.cpp`, plus `tests/test_wav.cpp` |
| change how frames are sized or hopped | `src/main.cpp` lines 26 to 33 |
| change the window function | `analyze_spectrum()` in `src/spectrum.cpp` |
| change the 50 and 60 Hz harmonic logic or thresholds | `measure_harmonics()` and the defaults in `include/humtrace/spectrum.hpp` |
| add a new per-frame measurement | `analyze_tone_timeline()` or a new timeline function, then the struct in `spectrum.hpp`, then `report.cpp` |
| change the JSON fields | `src/report.cpp` and `include/humtrace/report.hpp`. Update `schema/analysis-report-v1.schema.json` and `docs/modules/report-schema-v1.md`. Bump the schema version for breaking changes |
| change the printed summary | `src/main.cpp` output section |
| add a test | `tests/test_*.cpp` (also add the target to the Makefile `test` rule) |

## Gotchas for navigation

- `Makefile` builds each test with an explicit source list. A new source file must be added to the right rule.
- The `wav` and `spectrum` modules do not know about the CLI or JSON.
