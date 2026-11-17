# Beta limitations

- Only RIFF/WAVE with integer PCM at 8/16/24/32 bits or IEEE float at 32/64 bits is accepted.
- The CLI reads audio in bounded rolling windows, while its result timeline and JSON serialization remain in memory and grow with duration. The convenience `decode_wav` API also retains all samples.
- The command line analyzes complete overlapping windows across the recording, with a frame size up to 65,536 samples and a 50% hop.
- Each frame independently reports the strongest non-DC FFT bin. Separately, up to N local peaks per frame are associated into prototype tracks (CLI and JSON only). Track settings are synthetic-tested, not validated on real recordings, and the result is not a validated interference detector; a musical tone or other signal may be strongest or form a track.
- The Welch PSD is reported in dBFS^2/Hz (power per hertz), a different scale from the peak-amplitude dBFS elsewhere; do not compare the two directly. Its 50/60 Hz prominence flags are measurement aids tested on synthetic signals only, and the noise-floor settings are not validated on real recordings.
- The Hann-windowed FFT provides nominal resolution of sample rate divided by window length. Frequency and level accuracy need broader fractional-bin and noisy-signal validation.
- The JSON report (schema 1.1) records the input SHA-256, size, and application version, but it is not a chain-of-custody report: it is unsigned and the hash is computed by the same tool.
- The Qt GUI shows a findings table, harmonic-support timeline and per-frame spectrum only. No waveform, spectrogram, GUI display of peak tracks, GUI comparison, ENF matching, non-WAV codecs, or plugin exists. The GUI has not been tested with a screen reader or on Windows. Multi-file comparison is CLI-only text.
- No validation corpus of real recordings exists; false-positive and false-negative rates are unknown.
- Measurements in dBFS do not establish acoustic SPL, line voltage, ground-loop presence, hardware identity, location, or recording time.
