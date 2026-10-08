# Beta v0.1 release checklist

Current status: **not release-ready**. The offline streaming WAV/core/CLI and preliminary JSON report are implemented. Linux, Windows, and sanitizer CI passed on streaming commit `4727aa2` (run `37762099089`). There is no desktop GUI, installer, or release artifact.

- [ ] GUI is navigable, responsive, and shows progress/cancellation.
- [ ] Audio loading supports advertised formats; long-file analysis and report serialization have validated memory bounds.
- [ ] Mono/stereo findings include verified frequency, level, time, harmonics, noise-floor/SNR evidence, and qualified explanation.
- [ ] Spectrum, waveform, spectrogram, and finding timeline are implemented and visually checked.
- [ ] Multi-file comparison is implemented and reproducible.
- [ ] Versioned JSON measurement export is extended with input hashes, provenance/build metadata, warnings, and schema compatibility tests.
- [ ] Invalid/unsupported input handling and source preservation are tested.
- [ ] Labeled validation corpus demonstrates documented false-positive/false-negative and measurement-error results.
- [ ] Required license and third-party notices are reviewed and included.
- [ ] Linux and Windows native builds and packaged artifacts are tested.
- [ ] CI, sanitizer, static checks, dependency review, and release notes are complete.
- [ ] User guide and known limitations match the actual shipped build.

The 50/60 Hz evidence must not be presented as proof of a ground loop, location, device, recording date, or authenticity.
