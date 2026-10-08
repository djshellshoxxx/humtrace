# Beta v0.1 release checklist

Current status: **not release-ready**. The offline WAV/core/CLI and preliminary JSON report are implemented. Linux and Windows CI passed on the prior interpolation checkpoint; report integration and the new sanitizer job still need remote verification.

- [ ] GUI is navigable, responsive, and shows progress/cancellation.
- [ ] Audio loading supports advertised formats and long files with bounded memory.
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
