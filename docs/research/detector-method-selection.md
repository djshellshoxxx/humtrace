# Detector method selection and validation notes

**Reviewed:** 2026-10-08. This is a method-selection note for measurable signal behavior, not a claim that a spectrum identifies a physical interference source.

## Recommended analysis layers

1. **Per-frame spectrum for inspection.** Keep the existing Hann-windowed one-sided amplitude spectrum for a cursor-readable view. Store the exact normalization, frame length, hop, and bin spacing. A zero-padded display may interpolate a curve, but must not imply better resolving power than the actual observation window.
2. **Welch PSD for stable noise and narrowband evidence.** Welch averages modified periodograms from overlapping segments. It reduces estimator variance at the cost of frequency resolution when segment length is shortened. Treat PSD density (`dBFS²/Hz` or a clearly defined dBFS reference) separately from peak amplitude (`dBFS`) and power spectrum (`dBFS²`); do not put them on one unlabeled scale. Use Hann with 50% overlap as the starting benchmark, not a fixed universal optimum. Compare mean and median averaging for transient-contaminated recordings. SciPy documents the method, scaling modes, and this tradeoff, and is an independent test oracle rather than a runtime dependency. [1]
3. **Local peak evidence.** Detect local maxima on a linear power or dB representation with a documented neighborhood. Record peak height, width, local prominence, nearby floor estimate, and competing peaks. Prominence is the height above a surrounding contour; a bounded neighborhood gives local rather than global prominence, which changes its meaning and must be recorded. [2][3]
4. **Candidate harmonic families.** Search a configurable frequency interval around a candidate fundamental, then inspect integer multiples with tolerance derived from measured resolution and the estimator's validated error. Record each partial's expected frequency, measured peak, distance, level, local floor, prominence, and threshold outcome. Do not force a candidate to exactly 50 or 60 Hz when observed frequency differs.
5. **Temporal persistence.** Feed per-frame peak observations to the track assembler in `temporal-tracking.md`; evaluate duration, gap count, frequency drift, amplitude variation, and channel distribution. Candidate support must not be collapsed into one probability until calibration data exists.

## Numerical and interpretive requirements

- Keep amplitude spectrum and PSD estimators distinct. A sinusoid's peak amplitude can be measured using coherent-gain correction; noise power uses window-energy normalization and bandwidth-aware integration.
- Detrending/DC removal is a setting. The default should remove the mean from each frame for AC interference inspection, while retaining a separately reported DC statistic. Never silently high-pass filter the waveform.
- Estimate local noise by excluding a guard region around the candidate and using robust bins/statistics; state the exclusion width in Hz and bins. Compare median, trimmed mean, and robust quantile behavior on broadband, colored, tonal, and impulsive backgrounds.
- Apply parabolic interpolation only to a valid local peak with suitable neighbor values. Keep raw-bin result and interpolated result. Mark values as unresolved or uncertain when peaks are flat, clipped, close, or below floor.
- Report estimates and deterministic thresholds as evidence fields, not probability. “Strong”, “likely”, and percentages require calibrated validation and documented operating conditions.
- Per-channel values remain separate. Optional cross-channel coherence is separate evidence and does not identify the coupling mechanism.

## Required experiment matrix before thresholds are accepted

Vary SNR, signal duration, sample rate, frame length/hop, fractional-bin location, frequency drift, harmonics present/absent, close tones, clipping, DC offset, colored noise, transients, codec/resampling, stereo gain/phase, and interfering music/speech. Report frequency error, amplitude error, event detection precision/recall, false alarms per hour, and missed-event duration. Select thresholds on development recordings; freeze them before evaluating held-out recordings.

## Sources

1. SciPy, [`signal.welch`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html), method, scaling and averaging parameters; P. Welch, “The use of the fast Fourier transform for the estimation of power spectra,” *IEEE Trans. Audio Electroacoust.*, 15, 70–73 (1967), linked from the reference.
2. SciPy, [`signal.find_peaks`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.find_peaks.html), local maxima and measurable properties.
3. SciPy, [`signal.peak_prominences`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.peak_prominences.html), contour-based prominence and bounded-window consequences.
4. SciPy, [`signal.coherence`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.coherence.html) and [`signal.csd`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.csd.html), cross-spectral measurements.
5. Existing HumTrace [DSP research](dsp-algorithms.md), [taxonomy](interference-taxonomy.md), and [validation-corpus specification](../specs/validation-corpus.md).
