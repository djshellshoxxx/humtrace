# Prototype limitations

- Only RIFF/WAVE with integer PCM at 8/16/24/32 bits or IEEE float at 32/64 bits is accepted.
- The CLI reads audio in bounded rolling windows, while its result timeline and JSON serialization remain in memory and grow with duration. The convenience `decode_wav` API also retains all samples.
- The command line analyzes complete overlapping windows across the recording, with a frame size up to 65,536 samples and a 50% hop.
- Each frame independently reports the strongest non-DC FFT bin. The bins are not associated into persistent tracks and the result is not a validated interference detector; a musical tone or other signal may be strongest.
- The Hann-windowed FFT provides nominal resolution of sample rate divided by window length. Frequency and level accuracy need broader fractional-bin and noisy-signal validation.
- The CLI exports a preliminary versioned JSON measurement report; it is not a provenance or chain-of-custody report and currently omits input hashes and build/decoder versions.
- No desktop GUI, persistent peak tracking, multi-file comparison, ENF matching, broad codec coverage, or plugin target exists yet.
- Measurements in dBFS do not establish acoustic SPL, line voltage, ground-loop presence, hardware identity, location, or recording time.
