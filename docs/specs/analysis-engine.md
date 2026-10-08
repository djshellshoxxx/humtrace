# Analysis engine specification

**Status:** implementation specification for the next measurement-core stage. Existing dominant-bin and nominal-harmonic code is a prototype, not a validated detector.

## Scope and contracts

Input is one decoded channel plus sample rate and an immutable `AnalysisSettings` object. Output is per-frame measurements with frame start/end samples, method version, units, settings digest, and warnings. The engine has no GUI, filesystem, device, or source-attribution dependency. It must not combine channels implicitly.

Minimum settings: frame length, hop, analysis window, PSD segment length/hop, detrend choice, frequency band, absolute floor, local-prominence rule, peak separation, harmonic tolerance, and maximum report density. Validate all parameters before reading samples. Defaults are explicit and stored in reports.

## Processing stages

1. Preserve and report the channel's decoded sample range; detect non-finite samples and clipping.
2. Calculate frame mean/DC and RMS. For AC spectrum, subtract mean only when configured; do not alter the input buffer.
3. Produce a coherent-gain-corrected one-sided peak amplitude spectrum for tone inspection and a window-energy-normalized PSD for noise analysis. Each has a separate unit and calibration formula.
4. Find candidate local maxima in the configured band. For each candidate, retain raw bin, interpolated frequency, amplitude, PSD, width, nearest local floor, prominence, SNR-like ratio with named definition, and validity flags.
5. Group harmonic candidates around measured fundamentals and separately evaluate nominal 50 and 60 Hz hypotheses. Search tolerance is bounded by `max(configured_fractional_tolerance_hz, validated_resolution_multiple * bin_spacing)` and clamped so adjacent harmonic regions do not overlap.
6. Emit evidence components. The engine does not emit a probability, device identity, or confirmed physical cause.

## Measurement definitions

- `peak_dbfs`: 20 log10 of peak amplitude relative to normalized full scale, with a floor value plus a separate `below_numeric_floor` flag.
- `rms_dbfs`: 20 log10 of RMS sample amplitude relative to the maximum representable normalized sample magnitude 1.0; a full-scale sine therefore reads about −3.01 dBFS RMS under this convention. Label the reference in help/report metadata.
- `psd_fs2_per_hz`: one-sided PSD in normalized sample-amplitude-squared per Hz, using window-energy normalization. `psd_dbfs_per_hz` is `10 log10(psd_fs2_per_hz / (1 FS²/Hz))`. Integrating the linear PSD over a band gives normalized sample power; convert that integrated value to dBFS re 1 FS². State this reference and one-sided convention in the report so PSD and peak-amplitude dBFS are never conflated.
- `prominence_db`: candidate level minus a specified robust local baseline in dB; also expose baseline method and neighborhood.
- `frequency_hz`: raw-bin frequency and interpolated estimate as separate fields. Uncertainty is resolution/validation based, not just FFT bin spacing.
- `harmonic_support`: count of partials meeting recorded criteria, plus the per-partial evidence table; never serialize only the count.

## Failure behavior

Reject invalid sample rate, empty/short frames, impossible settings, unsupported frame sizes in the selected FFT backend, and non-finite decoded values. Return explicit warnings for clipping, low SNR, competing peaks, insufficient resolution, and boundary peaks. Silence produces no frequency estimate. No field may contain NaN or infinity; JSON representation uses `null` plus a status field.

## Performance

Precompute windows and FFT plans per settings key. Reuse buffers. Avoid per-bin heap allocation. Limit candidate count deterministically. Benchmarks must report wall time, peak working memory, sample rate, channels, duration, frame/hop settings, compiler/build, and CPU. Avoid parallelizing channels until cancellation, determinism, and UI responsiveness are measured.

## Tests and migration

Use analytical sine/noise fixtures and SciPy reference outputs with fixed versions/settings. Verify amplitude and PSD independently, off-bin frequency error across multiple windows, local prominence in colored noise, DC handling, clipping, close tones, and boundary behavior. Module extraction includes public header, units, algorithm/version note, test fixtures, and no-GUI dependency. See [detector research](../research/detector-method-selection.md) and [validation corpus](validation-corpus.md).
