# Welch power spectral density

The shared pipeline feeds each channel's decoded samples once to a bounded-memory Welch estimator. FFT display windows overlap; only their newly decoded suffix feeds Welch, including the final partial hop. No resampling or downmix occurs.

Settings: power-of-two segment size 4–1048576 (default 4096), hop 1–segment size (default 2048). Set both CLI flags when choosing a segment below the default hop. Invalid settings fail before analysis. The desktop currently uses defaults.

The symmetric Hann estimator averages one-sided periodograms normalized by sample rate and window energy. DC and Nyquist are not doubled. Raw DC is retained; the separate peak-amplitude spectrum removes frame means. Units are dBFS^2/Hz, not peak-amplitude dBFS, SPL, or voltage.

Only complete segments enter the average. Reports record segment/hop sizes, bin spacing, segment count, unique samples processed, broadband median, and nominal 50/60 Hz harmonic measurements. Short inputs have a null PSD summary. JSON null represents non-finite log values such as exact silence; it does not mean measured zero power.

Harmonic floors use median interior neighbors within 8 bins, excluding a 2-bin guard. Prominence uses the stronger bracketing bin, at the pipeline minimum prominence threshold. Nominal fundamentals need at least two bins of resolution; an empty neighborhood cannot establish prominence. These descriptive measurements do not identify a physical source.

Synthetic tests cover analytic white-noise density, Parseval scaling, bin-centered tone levels, variance reduction, deterministic chunking/reset, partial segments, silence, input validation, coarse resolution, missing floors, and pipeline sample counts. CLI tests validate configurable settings and report schema.

Ported from `feat/welch-psd` commit `fd9f87e`, adapted to the desktop branch's pipeline and report model without replacing existing tracking, integrity, cancellation, or GUI code.
