# Beta limitations

- Input: RIFF/WAVE only (PCM 8/16/24/32-bit, IEEE float 32/64-bit, WAVE_FORMAT_EXTENSIBLE with full valid bits). No RF64, AIFF, FLAC, MP3.
- Analysis uses a Hann-windowed FFT, window up to 65,536 samples, 50% hop. Frequency resolution is sample rate / window; peak interpolation is parabolic. Accuracy is verified on synthetic signals only ([results](../validation/synthetic-results.md)), not on real recordings.
- Tracks associate up to eight prominent peaks per frame by frequency; they follow no split/merge lineage, and tracks seen in fewer than three frames are hidden by default. "Persistence" wording is descriptive and is not a validated detector.
- Results (frames, tracks, spectrogram rows) stay in memory: roughly 25 MB per hour per channel at 8 kHz. A configurable cap (default 512 MiB) refuses over-long runs up front rather than downsampling.
- The JSON report records measurements, settings, and an input SHA-256. It is not a chain-of-custody or provenance record and has no build/decoder version fields or warnings list yet.
- Comparison is an independent summary only: no time alignment, no resampling, no cross-spectral analysis. Channels pair by identical number.
- The GUI has a custom-drawn spectrogram/waveform/spectrum; each has a text/table alternative (findings table, spectrum table, inspector) and an accessible summary, but screen-reader behavior has not been verified with Narrator or AT-SPI tools.
- The CLI on Windows expects ASCII paths (the GUI accepts any name). Windows builds are produced in CI but have not been tested on a clean machine.
- Measurements in dBFS do not establish acoustic SPL, line voltage, ground-loop presence, hardware identity, location, authenticity, or recording time.
- Not implemented: ENF analysis, diagnostic guidance, signature library, plugins, playback.
