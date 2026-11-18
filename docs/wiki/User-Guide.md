# User Guide

```sh
make
build/humtrace recording.wav                        # summary and per-frame peaks
build/humtrace recording.wav --json analysis.json   # versioned JSON measurements
```

Reads RIFF/WAVE PCM (8, 16, 24, 32-bit) and IEEE float (32, 64-bit). Reports the sample rate, channels, bit depth, duration and FFT resolution.

**Read the results as measurements.** Levels are peak dBFS from normalised decoded samples, not SPL or voltage. A match to a 50/60 Hz pattern does not prove a ground loop or identify a device or location (README, "Measurement interpretation"). JSON output is a measurement export, not a chain-of-custody report.
