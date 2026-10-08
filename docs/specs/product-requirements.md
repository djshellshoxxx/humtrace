# HumTrace product requirements and delivery plan

## Beta v0.1 target

Windows and Linux desktop tool for inspecting audio recordings for repeatable electrical and electronic interference. A result must show measurements and settings, identify plausible patterns with qualified language, and avoid asserting a physical cause from frequency alone.

## v0.1 acceptance requirements

- Load PCM/float WAV; later add AIFF, FLAC and MP3 through a reviewed decoder dependency.
- Analyze mono and stereo independently, with bounded memory for long recordings.
- Provide overview metadata, waveform, spectrum, spectrogram, and finding timeline.
- Detect persistent narrowband tones and plausible 50/60 Hz harmonic candidates with frequency, level, local noise estimate, persistence and evidence components.
- Expose analysis settings and reproducibly export a versioned report.
- Handle invalid files and cancellation without modifying the source recording.
- Maintain a deterministic synthetic test suite with measurable frequency/level tolerances and false-positive negatives.
- Package and natively verify Windows and Linux artifacts before claiming either platform complete.

Detailed acceptance behavior is linked from the [design index](../design/index.md). In particular, v0.1 also requires input/report integrity metadata with bounded claims, keyboard and screen-reader access to measurements, no silent resampling/downmixing/alignment, and a validation corpus split by recording session/source to prevent leakage.

## Staged build order

1. **Core measurement (started):** streaming RIFF/WAVE PCM/float batches, one-sided spectrum, dBFS units, parabolic peak interpolation, and test harness.
2. **Interference measurements (partial):** local spectral-floor prominence and 50/60 harmonic-bin measurements are implemented; Welch PSD, off-nominal candidate tracking, and validated real-recording fixtures remain.
3. **Timeline (partial):** overlapping per-channel dominant-bin timeline is implemented; persistent candidate association, drift estimates, and event intervals remain.
4. **File workflow (partial):** streaming decode and preliminary JSON report schema are implemented; cancellation/progress and multi-file comparison remain.
5. **Desktop application:** background analysis, responsive UI, waveform/spectrum/spectrogram/timeline, result explanation.
6. **Release hardening:** CI, sanitizer jobs, dependency/license audit, installers, native platform verification, user guide, beta notes.

Optional plugin, ENF matching, hardware troubleshooting suggestions, and signature library are outside the first release gate unless the core beta is already verified.

Deferred feature specs: [ENF](enf-analysis.md), [diagnostic guidance](diagnostic-guidance.md), [signature library](signature-library.md), and [plugin adapter](plugin-adapter.md). Each remains gated by separate research, validation, and licensing review; none is implied to be implemented by its specification.

Toolkit direction is provisionally Qt 6 Widgets, subject to the prototype gates and per-module license/SBOM review in [GUI toolkit decision research](../research/gui-toolkit-decision-research.md). This does not set the HumTrace product license.

## Initial non-goals

- Claiming a detected hum proves a ground loop or identifies equipment.
- Using ENF as a date, location, authentication, or provenance claim.
- Changing or cleaning the source audio.
- Publishing automated source-attribution probabilities without validated labeled data.
