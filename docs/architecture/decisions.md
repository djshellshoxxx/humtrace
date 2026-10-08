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

## D-006: Defer desktop toolkit choice until license and accessibility review

- **Status:** open
- **Problem:** GUI delivery needs a framework, but framework terms affect packaging and project licensing.
- **Decision:** complete the interaction spec and compare supported toolkit options before adding a runtime dependency.
- **Alternatives:** JUCE, Qt, wxWidgets, SDL2 with Dear ImGui.
- **Consequences:** GUI implementation is the current major product blocker; analysis core remains usable while the decision is open.
