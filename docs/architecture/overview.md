# HumTrace architecture overview

## Current data flow

`RIFF/WAVE stream -> decoder -> interleaved normalized samples -> per-channel overlapping frames -> Hann/radix-2 spectrum -> dominant-bin/harmonic timeline -> versioned JSON report`

## Modules

| Module | Public surface | Constraints |
|---|---|---|
| WAV decoder | `humtrace::decode_wav(std::istream&)` | File I/O only; no DSP; v0.1 prototype loads a complete RIFF file. |
| Spectrum | `analyze_spectrum`, `strongest_peak`, `measure_harmonics`, timeline analyzers | Pure computation; caller owns buffers; no audio callback use; FFT frame size must be a power of two. |
| Report | `serialize_report_json` | Serializes measurement results and settings; maps non-finite floats to JSON `null`; no file I/O. |
| CLI | `humtrace <input.wav>` | Presentation and orchestration only; must not alter input files. |

## Planned module boundaries

1. `audio_io`: streaming decode, format metadata, channel iteration.
2. `dsp`: windowing, FFT, PSD, peak detection, harmonic grouping, tracking, measurements.
3. `analysis`: per-channel timelines, comparison, evidence components and report model.
4. `report`: versioned JSON and human-readable outputs.
5. `app`: desktop UI and background job lifecycle.
6. `plugin`: optional adapter over the analysis library; no duplicated DSP.

Keep the analysis core independent of JUCE and GUI state. Never run long-file analysis on a real-time audio callback. Every result should preserve units, analysis settings, channel identity, time range, and method/version metadata.

## Initial decisions

- Use a self-contained radix-2 FFT for the first dependency-free prototype; benchmark and compare with an established FFT library before release.
- Use a Hann window and coherent-gain-corrected one-sided peak-amplitude spectrum; apply parabolic interpolation in dB around the dominant bin for the reported peak estimate.
- Report `dBFS`; no calibration metadata exists to justify SPL or voltage.
- Label observed frequency energy directly; keep possible electrical-source explanations separate.
- Require both an absolute level and local spectral prominence for initial harmonic-bin support; defer probability/confidence percentages until there is a representative labeled corpus and held-out validation.

## Known architecture gaps

The current decoder does not support RF64, WAVEFORMATEXTENSIBLE, compressed codecs, streaming, or bounded-memory operation. The current timeline independently reports each frame's strongest bin; it does not associate peaks into persistent tracks and does not yet expose PSD, noise-floor estimates, or calibrated detection evidence.
