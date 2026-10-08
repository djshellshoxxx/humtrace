# HumTrace DSP algorithms: initial research

**Scope:** defensible v0.1 analysis of audio-file interference, especially persistent narrowband tones and mains-related harmonic candidates. This is an engineering recommendation, not a claim that any measured tone identifies its physical cause. Sources were accessed 2026-10-08.

## Decode and measurement assumptions

Use a maintained audio decoder and treat WAV as a chunked RIFF container: locate and validate format and data chunks rather than assuming fixed header offsets. Retain the decoder's sample rate, channel count, sample representation, valid bits, and channel mask where present. `WAVEFORMATEXTENSIBLE` distinguishes container width from valid sample bits and carries channel-position information; this matters for multichannel PCM. Convert decoded samples to floating point for DSP without changing the original file. Reject malformed, unsupported, or truncated input with a precise error. Do not silently resample or downmix for the primary analysis; if a derived signal is made, record the transformation. [1][2]

For v0.1, prioritize uncompressed PCM and IEEE-float WAV through the selected decoder. Record decoded format and any decoder/resampling decisions in the report. Keep amplitude measurements tied to the decoder's normalized samples; without calibration metadata, report dBFS, not acoustic SPL or electrical voltage.

## FFT, STFT, and Welch PSD

Use a real-input FFT for efficiency. The one-sided real-input spectrum contains bins from DC through Nyquist; FFT libraries may use unnormalized transforms, so define and test scaling explicitly before presenting amplitude or power. [3]

- **Whole-selection spectrum:** a windowed FFT is useful for inspection but is a single estimate and can hide intermittent events.
- **Welch PSD:** divide a selection into overlapping, windowed frames, compute modified periodograms, and average them. Use this for stable overall noise/tone estimates; report whether values are PSD (power per Hz) or spectrum power and keep units explicit. [4]
- **STFT:** retain frame-by-frame complex or power spectra to map frequency and level over time. The frame length sets the time/frequency tradeoff; hop size sets update spacing, not independent frequency resolution. [5]

Use a Hann window as a sensible default for general tone inspection, with a documented alternative such as a flat-top window for amplitude-focused inspection if needed. Windowing reduces leakage from frame-boundary discontinuities but changes main-lobe width and sidelobe behavior; no window gives best amplitude accuracy, narrowest separation, and lowest leakage simultaneously. Nominal bin spacing is `fs / N`; zero-padding interpolates the displayed spectrum but does not improve the resolving power set by the observation duration/window. [5][6]

Expose frame duration and hop in seconds in the UI/report. A useful initial analysis can offer a longer frame for close 50/60 Hz discrimination and a shorter frame for event timing, then validate defaults against realistic SNR and drift. Avoid asserting universal settings: low-frequency separation needs sufficiently long observations, while fast changes need shorter frames.

## Peak detection and tracking

Detect local maxima in a log-power spectrum, then filter candidates by prominence over a locally estimated noise floor, minimum width/height, and exclusion of DC and unreliable edge bins. Peak prominence, width, and separation are measurable controls; fixed absolute thresholds alone are fragile across recordings. [7]

For a peak estimate finer than the FFT grid, use a documented local interpolation around the maximum (e.g., parabolic interpolation of log magnitudes) and test its bias across fractional-bin offsets, SNR, and nearby tones. The current prototype applies parabolic interpolation to dB magnitudes for the strongest non-DC peak; its test fixture measures a 60.4 Hz tone within 0.1 Hz and 0.25 dB. This does not replace testing across varied SNR, bin offsets, and neighboring tones. Preserve the interpolated estimate separately from the underlying FFT grid. Track candidates frame to frame using gated frequency distance plus level continuity; allow missed frames and track birth/death rather than switching to whichever peak is currently strongest. Persistent candidate association remains future work.

## Mains and harmonic measurements

Treat 50 Hz and 60 Hz as nominal search hypotheses, not exact constants; IEC standard-voltage material recognizes both standard system frequencies. Search a configurable neighborhood around each nominal and permit slow drift. For candidate fundamental `f0`, measure energy near `k*f0` for integer harmonics below Nyquist, and report each harmonic's frequency, level, local noise estimate, bandwidth, persistence, and drift. [8]

Score harmonic support from multiple observed partials, frequency-ratio consistency, persistence, and shared drift. Keep the measurements visible even when classification is uncertain. A 50/60 Hz tone or harmonic series is evidence of periodic energy only: it does not prove a ground loop, a particular appliance, a room, or a grid connection. Harmonics can be absent, masked, introduced by nonlinear distortion, or produced by unrelated sources. Switching supplies and clocks may create narrowband or harmonic families unrelated to mains.

ENF extraction is a separate, higher-evidence feature: it requires a sufficiently strong, trackable component and, for matching/timestamp claims, a valid synchronized reference sequence and validated procedure. Research identifies recording length, temporal resolution, and SNR among factors affecting matching reliability. HumTrace v0.1 should present ENF-like drift as a measured candidate only; do not infer recording date, location, authenticity, or editing from it. [9]

## Detection evidence and limitations

Avoid an uncalibrated percentage called “confidence.” For each finding, show an evidence grade or component scores derived from explicit measurements: prominence above local floor, SNR, harmonic support, track continuity, duration, and frequency stability. Keep source attribution as a separate, qualified hypothesis. Calibrate any probability only against a representative labeled corpus with held-out recordings and reported false-positive/false-negative rates; synthetic-only data is not enough.

Expect reduced or misleading detection after lossy encoding, denoising, high-/low-pass filtering, resampling, clipping, strong masking audio, or short excerpts. A missing signature is not evidence the interference was absent before processing. Report analysis bandwidth, frame/window settings, thresholds, and processing caveats so a result can be reproduced.

## Multichannel measures

Analyze every channel independently first; do not average channels before detection, because opposite phase can cancel and a signature may be present in only one channel. Report per-channel level, frequency, persistence, and track. For channel pairs, optional Welch magnitude-squared coherence and cross-spectrum phase can describe whether components are shared and their relative phase over the selected band. These measures are meaningful only for synchronized channels and adequate signal energy; high coherence or stable phase does not identify the cause. [10]

## Practical validation plan

1. **Decoder fixtures:** generate/read PCM 8/16/24/32-bit and float WAV fixtures, mono/stereo and extensible multichannel with valid-bit/channel-mask cases; verify sample count, rate, channel order, normalized amplitude, and malformed/truncated-file errors.
2. **Known-signal DSP fixtures:** use deterministic tones at exact-bin and fractional-bin frequencies, 50/60 Hz fundamentals and partials, missing/weak harmonics, non-harmonic tones, chirps/drift, bursts, DC offset, silence, and broadband noise. Check frequency/level tolerances against analytical values.
3. **Robustness sweeps:** vary SNR, duration, frame/hop, window, tone spacing, drift rate, channel phase, clipping, and interfering speech/music. Measure detection precision/recall and frequency/amplitude error; include difficult negatives such as bass notes near mains frequency.
4. **Transformation checks:** resample and encode/decode representative files, apply filtering and gain changes, then document which measurements should remain invariant and where performance degrades.
5. **Regression corpus:** keep synthetic generators and a small, rights-cleared labeled corpus with provenance. Freeze expected results and test output determinism. Tune thresholds on development data, then publish metrics on held-out recordings.

## Sources

1. Microsoft Learn, [Resource Interchange File Format (RIFF)](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-), accessed 2026-10-08.
2. Microsoft Learn, [WAVEFORMATEXTENSIBLE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible), accessed 2026-10-08; [WAVEFORMATEX](https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatex), accessed 2026-10-08.
3. FFTW 3.3.11 manual, [One-dimensional DFTs of real data](https://www.fftw.org/fftw3_doc/One_002ddimensional-DFTs-of-Real-Data.html), accessed 2026-10-08.
4. SciPy 1.18 manual, [`scipy.signal.welch`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html), accessed 2026-10-08.
5. SciPy 1.18 manual, [`scipy.signal.stft`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.stft.html) and [spectral analysis tutorial](https://docs.scipy.org/doc/scipy/tutorial/signal.html), accessed 2026-10-08.
6. SciPy 1.18 manual, [`scipy.signal.windows.hann`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.windows.hann.html), accessed 2026-10-08.
7. SciPy 1.18 manual, [`scipy.signal.find_peaks`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.find_peaks.html), accessed 2026-10-08.
8. IEC, [IEC 60038:2009 + AMD1:2021 CSV](https://webstore.iec.ch/en/publication/72877), accessed 2026-10-08.
9. Guang Hua et al., “Factors affecting forensic electric network frequency matching – A comprehensive study,” *Digital Communications and Networks* 10(4), 1121–1130 (2024), [DOI / publisher record](https://doi.org/10.1016/j.dcan.2023.01.009), accessed 2026-10-08. See also [publisher abstract](https://www.sciencedirect.com/science/article/pii/S2352864823000226).
10. SciPy 1.18 manual, [`scipy.signal.coherence`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.coherence.html) and [`scipy.signal.csd`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.csd.html), accessed 2026-10-08.
