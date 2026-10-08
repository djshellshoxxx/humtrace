# Spectrum core module

## Purpose

Provide deterministic offline frequency and level measurements that can be reused by the CLI, desktop application, report generator, or another Circuit Drift Labs project.

## Files and public API

- `include/humtrace/spectrum.hpp`
- `src/spectrum.cpp`
- `analyze_spectrum(samples, sample_rate_hz)` returns a one-sided, Hann-windowed, coherent-gain-corrected peak-amplitude spectrum.
- `strongest_peak(spectrum)` reports the largest non-DC bin with local parabolic interpolation in dB when both adjacent bins are usable.
- `measure_harmonics(spectrum, nominal_hz, support_threshold_dbfs, max_harmonics)` returns the selected bins and threshold-support count.
- `analyze_tone_timeline(samples, sample_rate_hz, frame_size, hop_size)` independently measures each complete frame.
- `analyze_interference_timeline(...)` reports each frame's dominant bin plus nominal 50/60 Hz harmonic-bin measurements.

## Contract and limits

- C++20 standard library only.
- Input samples are finite normalized floating-point values; reported level is dBFS.
- FFT size is power-of-two and at least four; timeline hop must be nonzero.
- No audio callback, GUI, file I/O, or source-classification dependency.
- Harmonic support requires both an absolute dBFS threshold and local prominence over a median spectral floor. The initial floor uses bins 3–8 away from the target bin on either side; this is a prototype heuristic that still needs calibration against real recordings.
- Thresholded support is a measurement summary, not a confidence score or proof of mains coupling.
- Silence is represented as frequency 0 Hz with level `-inf` dBFS, meaning no non-DC energy was measured.
- Interpolated peak frequency is a local estimate; the returned `SpectrumBin` grid remains unchanged, and harmonic measurements still use nearest FFT bins.
- The timeline does not associate peaks across frames and makes no persistence claim.

## Test and migration

Run `make test`; `tests/test_spectrum.cpp` checks exact-bin and fractional-bin frequency/level, invalid FFT size, harmonic measurements, frequency changes across frames, local prominence over deterministic noise, and silence. To migrate, copy the header and source, preserve the namespace and units, add the source to the consuming build, then port the test file and compile with C++20. The caller must supply a sample rate and decide its own framing and result interpretation.

## Reuse candidates

Useful for tone meters, spectral diagnostics, audio restoration experiments, and test-signal inspection. A future shared-catalog release should version the API and add fractional-bin, noisy-signal, DC/Nyquist, and performance tests before being considered stable.
