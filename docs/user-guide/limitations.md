# Prototype limitations

- Only RIFF/WAVE with integer PCM at 8/16/24/32 bits or IEEE float at 32/64 bits is accepted.
- Files are loaded into memory; large-file analysis is not yet safe or bounded.
- The command line analyzes complete overlapping windows across the recording, with a frame size up to 65,536 samples and a 50% hop.
- Each frame independently reports the strongest non-DC FFT bin. The bins are not associated into persistent tracks and the result is not a validated interference detector; a musical tone or other signal may be strongest.
- The Hann-windowed FFT provides nominal resolution of sample rate divided by window length. Frequency and level accuracy need broader fractional-bin and noisy-signal validation.
- No desktop GUI, report export, temporal tracking, ENF matching, codec coverage, or plugin target exists yet.
- Measurements in dBFS do not establish acoustic SPL, line voltage, ground-loop presence, hardware identity, location, or recording time.
