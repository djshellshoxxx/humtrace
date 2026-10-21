# User Guide

## What it reads

RIFF/WAVE files with:

- integer PCM at 8, 16, 24 or 32 bits
- IEEE float at 32 or 64 bits

Anything else is rejected. Multi-channel files are analysed per channel.

## Run it

```sh
build/humtrace recording.wav                       # print the summary
build/humtrace recording.wav --json analysis.json  # also write a JSON report
```

Usage line: `humtrace <input.wav> [--json <report.json>]`.

## What it prints

- sample rate, channel count, bit depth and duration
- FFT resolution (sample rate divided by window length)
- per frame, the strongest non-DC spectral component and its start time
- nominal 50 Hz and 60 Hz harmonic measurements, with absolute level and local prominence

Frames use a Hann window and overlap by 50 percent. The frame size is the largest power of two that fits within the smaller of the file length and the sample rate, capped at 65,536 samples. A file shorter than 4 samples is refused.

## How to read the numbers

- **Levels are peak dBFS** from normalised decoded samples. They are not SPL, not line voltage and not a calibrated reading.
- **Harmonic thresholds.** A harmonic counts as supported when it is above a level threshold (default -60 dBFS) and above its neighbours by a local prominence margin.
- **Observed energy is kept separate from source attribution.** The report says what it saw. It does not say what caused it.
- A frame's strongest bin is just the strongest bin. A music tone can be strongest, and the tool does not know it is music.

## JSON report

`--json` writes a versioned report. The layout is in `docs/modules/report-schema-v1.md` and `schema/analysis-report-v1.schema.json`. It holds the input details, per-channel frames and harmonic candidates.

The report has `schema_version` 1.0 and does not include input hashes or the build and decoder versions. Do not use it as a chain-of-custody record.

## Limits

- Frequency and level accuracy are not yet validated for fractional bins or noisy signals.
- No persistent tracking across frames.
- No GUI, no multi-file comparison, no ENF matching, no broad codec support, no plug-in target.
- A result does not establish ground-loop presence, hardware identity, location or recording time.

See `docs/user-guide/limitations.md` for the full list.
