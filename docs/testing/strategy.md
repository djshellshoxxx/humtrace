# Testing strategy and current evidence

## Current automated tests

`make test` compiles with C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror` and runs:

- Spectrum frequency/level for a known exact-bin sine.
- Rejection of a non-power-of-two FFT input.
- Thresholded measurements for a two-harmonic synthetic signal.
- Per-frame nominal 50/60 Hz harmonic-bin measurements.
- Timeline timestamps and dominant-frequency change across two frames.
- Silence representation without a fabricated peak frequency.
- WAV metadata and 16-bit PCM normalization.
- 24-bit sign extension and odd chunk padding.
- Rejection of non-WAVE input.

The CLI was manually run against a generated 4096 Hz, 60/120 Hz, one-second WAV fixture. It reported a dominant 60 Hz component at -12.041 dBFS, 1 Hz/bin, and one complete frame.

AddressSanitizer and UndefinedBehaviorSanitizer builds of both test executables passed with `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer could not run in this container because its process-inspection attempt fails; this is an environment limitation, so leak checking remains open.

## Required before beta

- PCM 8/16/24/32 and IEEE float 32/64 fixtures; stereo, extensible, multichannel, malformed and truncated inputs.
- Fractional-bin, off-nominal 50/60 Hz, drift, close tones, broadband noise, silence, DC, bursts, clipping, and low-SNR tests.
- Precision/recall and frequency/amplitude error on labeled development and held-out recordings.
- CLI integration tests, stable report round-trip/schema tests, cancellation/progress tests, and long-file bounded-memory tests.
- Linux and Windows native CI; macOS only when a native environment is available.
- Sanitizer coverage where supported, dependency/license scan, and reproducible package smoke tests.

Do not use passing synthetic tests as proof of classifier accuracy or field performance.
