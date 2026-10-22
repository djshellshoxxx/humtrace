# Beta limitations

- Input: RIFF/WAVE only (PCM 8/16/24/32-bit, IEEE float 32/64-bit, WAVE_FORMAT_EXTENSIBLE with full valid bits). No RF64, AIFF, FLAC, MP3.
- Analysis uses a Hann-windowed FFT, window up to 65,536 samples, 50% hop plus one final window aligned to the end of the recording so no audio goes unmeasured (it overlaps the previous window by more than 50%). Frequency resolution is sample rate / window; peak interpolation is parabolic. Accuracy is verified on synthetic signals only ([results](../validation/synthetic-results.md)), not on real recordings.
- The 50/60 Hz harmonic-bin counts are non-specific: on real recordings, harmonically rich non-mains sounds (gongs, bells, voiced speech) can show several nominal harmonic bins above threshold, and speech/music yield many descriptive tracks ([real-recording cross-check](../validation/real-recordings.md)). Treat them as measurements, not hum detection.
- Tracks associate up to eight prominent peaks per frame by frequency; they follow no split/merge lineage, and tracks seen in fewer than three frames are not listed by default (counted in the report). "Persistence" wording is descriptive and is not a validated detector.
- Results (frames, tracks, spectrogram rows) stay in memory: roughly 25 MB per hour per channel at 8 kHz. A cap (`max_result_bytes`, default 512 MiB; an API setting with no CLI or GUI control yet) refuses over-long runs up front rather than downsampling.
- The JSON report records measurements, all result-affecting settings, application/method versions, warnings, and an input SHA-256. It is not a chain-of-custody or provenance record.
- Comparison is an independent summary only: no time alignment, no resampling, no cross-spectral analysis. Channels pair by identical number.
- The GUI has a custom-drawn spectrogram/waveform/spectrum; each has a text/table alternative (findings table, spectrum table, inspector) and an accessible summary, but screen-reader behavior has not been verified with Narrator or AT-SPI tools.
- The CLI and GUI use UTF-8 paths on Windows. Windows builds are produced in CI but have not been tested on a clean machine.
- Measurements in dBFS do not establish acoustic SPL, line voltage, ground-loop presence, hardware identity, location, authenticity, or recording time.
- Not implemented from the analysis-engine spec: RMS, per-track noise-floor/SNR evidence, measurement uncertainty, and a below-numeric-floor flag. Clipping is counted and warned about; a non-finite float sample aborts the analysis.
- The report has no generation timestamp (so identical inputs give byte-identical reports); it records application and method versions, input byte length, and all result-affecting settings.
- Not implemented: ENF analysis, diagnostic guidance, signature library, plugins, playback.
- Note names and cents are equal-tempered labels computed from the measured frequency; they say nothing about the source. Release binaries are unsigned; macOS builds are Apple silicon only and not notarized.

- Welch PSD uses complete Hann segments only (default 4096 samples, hop 2048); trailing partial segments are excluded and audio shorter than one segment has no PSD summary. Raw DC is retained in Welch, independently of the mean removal in peak-amplitude FFT frames. The broadband floor is a median of non-DC/non-Nyquist density bins, not calibrated sensor noise. Unresolved nominal harmonics and empty floor neighborhoods cannot establish prominence.
