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

## Staged build order

1. **Core measurement (started):** streaming RIFF/WAVE PCM/float batches, one-sided spectrum, dBFS units, parabolic peak interpolation, and test harness.
2. **Interference measurements (partial):** local spectral-floor prominence and 50/60 harmonic-bin measurements are implemented; Welch PSD, off-nominal candidate tracking, and validated real-recording fixtures remain.
3. **Timeline (partial):** overlapping per-channel dominant-bin timeline is implemented; persistent candidate association, drift estimates, and event intervals remain.
4. **File workflow (partial):** streaming decode and preliminary JSON report schema are implemented; cancellation/progress and multi-file comparison remain.
5. **Desktop application:** background analysis, responsive UI, waveform/spectrum/spectrogram/timeline, result explanation.
6. **Release hardening:** CI, sanitizer jobs, dependency/license audit, installers, native platform verification, user guide, beta notes.

Optional plugin, ENF matching, hardware troubleshooting suggestions, and signature library are outside the first release gate unless the core beta is already verified.

## Initial non-goals

- Claiming a detected hum proves a ground loop or identifies equipment.
- Using ENF as a date, location, authentication, or provenance claim.
- Changing or cleaning the source audio.
- Publishing automated source-attribution probabilities without validated labeled data.
