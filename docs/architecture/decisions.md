# HumTrace decision log

## D-001: Keep measurement modules independent of the GUI

- **Status:** accepted
- **Problem:** CLI, desktop UI, and optional plugins need the same analysis behavior.
- **Decision:** keep WAV reading, spectrum analysis, result structures, and report serialization independent of GUI state and JUCE.
- **Alternatives:** put analysis in the GUI application; use a plugin framework as the core dependency.
- **Consequences:** modules can be tested and reused independently; application orchestration must pass explicit settings/results.

## D-002: Report normalized levels as dBFS

- **Status:** accepted
- **Problem:** audio files do not provide a reliable acoustic or electrical calibration reference.
- **Decision:** report peak-amplitude dBFS from decoded samples; do not label values SPL or voltage.
- **Alternatives:** infer SPL from digital level; convert level to electrical magnitude without calibration.
- **Consequences:** results are reproducible in the digital domain but do not quantify room sound pressure or circuit voltage.

## D-003: Use a self-contained radix-2 FFT in the first core

- **Status:** provisional
- **Problem:** the initially empty repository needs a dependency-free measurement slice.
- **Decision:** implement a small radix-2 FFT and test scaling against analytical sine fixtures.
- **Alternatives:** FFTW, KissFFT, JUCE DSP, or another backend.
- **Consequences:** low setup cost and explicit normalization; power-of-two frame sizes only. Benchmark and compare before beta optimization.

## D-004: Treat 50/60 Hz as measured harmonic hypotheses

- **Status:** accepted
- **Problem:** tones and harmonics do not uniquely identify physical causes.
- **Decision:** report candidate-bin measurements and evidence thresholds separately from source attribution; do not use probability labels without a validation corpus.
- **Alternatives:** call a 50/60 series “ground hum”; output an uncalibrated confidence percentage.
- **Consequences:** findings stay bounded to measured energy and are less likely to overstate electrical diagnosis.

## D-005: Start with RIFF/WAVE PCM and IEEE float

- **Status:** accepted for prototype
- **Problem:** broad codecs add dependencies and license/build review before the core is validated.
- **Decision:** begin with seekable RIFF/WAVE PCM 8/16/24/32 and IEEE float 32/64; use `WavStreamReader` for bounded batches.
- **Alternatives:** FFmpeg or libsndfile from the start; support compressed formats.
- **Consequences:** the offline CLI can demonstrate streaming analysis without a runtime dependency. RF64, WAVEFORMATEXTENSIBLE, AIFF, FLAC, and MP3 remain unsupported.

## D-006: Use Qt 6 Widgets provisionally for the desktop application

- **Status:** provisional; implementation gated by prototype and module/license review
- **Problem:** GUI delivery needs a framework with a strong desktop shell, test support, custom plots, accessibility hooks, and Windows/Linux packaging.
- **Decision:** prototype Qt 6 Widgets with CMake. Use Core/Gui/Widgets and QPainter-based plot widgets; keep GUI types out of the C++ analysis core. Do not pull in Qt Graphs or other modules before checking their license and runtime dependencies.
- **Alternatives:** JUCE, wxWidgets, SDL2 with Dear ImGui; see [toolkit research](../research/gui-toolkit-decision-research.md).
- **Consequences:** Qt is a good technical fit for this offline measurement desktop workflow, but project licensing is still undecided and Qt module terms vary. Before adopting it for release, audit exact pinned module versions and SBOM, run Windows/Linux accessibility and packaging prototypes, and decide whether the selected terms fit the product's chosen license/distribution. If that gate fails, reopen this decision. No HumTrace license is selected here.
