# Beta limitations

- Only RIFF/WAVE with integer PCM at 8/16/24/32 bits or IEEE float at 32/64 bits is accepted.
- The CLI reads audio in bounded rolling windows, while its result timeline and JSON serialization remain in memory and grow with duration. The convenience `decode_wav` API also retains all samples.
- The command line analyzes complete overlapping windows across the recording, with a frame size up to 65,536 samples and a 50% hop.
- Each frame reports the strongest non-DC FFT bin, and a separate tracker associates up to a fixed number of peaks per frame into tone tracks (greedy nearest frequency, default tolerance 1.5 FFT bins, gaps up to 2 frames, at least 5 frames and 0.5 s (`min_frames` in the report is the effective value), at most 64 active and 4096 total tracks; `truncated` is set when limits are hit). Greedy association can merge, split or swap close or crossing tones. The result is not a validated interference detector; a musical tone or other signal may be strongest or tracked.
- `near_mains_multiple` flags tracks near n x 50/60 Hz (n <= 10). It is frequency proximity only and is not evidence of mains hum or any source.
- The Hann-windowed FFT provides nominal resolution of sample rate divided by window length. Frequency and level accuracy need broader fractional-bin and noisy-signal validation.
- The JSON report (schema 1.2) records the input SHA-256, size, and application version, but it is not a chain-of-custody report: it is unsigned and the hash is computed by the same tool.
- The Qt GUI has waveform, log-frequency spectrogram with track overlay, spectrum, findings and tracks tables, and a comparison dialog. None of it has been visually reviewed by a human, tested with a screen reader, or built/run on Windows (the Windows CI job is defined but unrun). No ENF matching, non-WAV codecs, or plugin exists.
- Comparison (CLI text/JSON and GUI) matches tracked frequencies across files by tolerance; it does not align time, correct sample-rate differences, or show a common source.
- No validation corpus of real recordings exists; false-positive and false-negative rates are unknown.
- Measurements in dBFS do not establish acoustic SPL, line voltage, ground-loop presence, hardware identity, location, or recording time.
