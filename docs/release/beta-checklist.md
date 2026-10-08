# Beta v0.1 release checklist

Current status: **not release-ready**. Only the first offline CLI/core prototype is implemented and Linux-local tests have run.

- [ ] GUI is navigable, responsive, and shows progress/cancellation.
- [ ] Audio loading supports advertised formats and long files with bounded memory.
- [ ] Mono/stereo findings include verified frequency, level, time, harmonics, noise-floor/SNR evidence, and qualified explanation.
- [ ] Spectrum, waveform, spectrogram, and finding timeline are implemented and visually checked.
- [ ] Multi-file comparison and versioned report export are implemented and reproducible.
- [ ] Invalid/unsupported input handling and source preservation are tested.
- [ ] Labeled validation corpus demonstrates documented false-positive/false-negative and measurement-error results.
- [ ] Required license and third-party notices are reviewed and included.
- [ ] Linux and Windows native builds and packaged artifacts are tested.
- [ ] CI, sanitizer, static checks, dependency review, and release notes are complete.
- [ ] User guide and known limitations match the actual shipped build.

The 50/60 Hz evidence must not be presented as proof of a ground loop, location, device, recording date, or authenticity.
