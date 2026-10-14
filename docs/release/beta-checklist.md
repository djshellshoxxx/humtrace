# Beta v0.1 release checklist

Current status: **feature-complete beta candidate; not release-ready**. Items marked [x] were verified in this repository's tooling on Linux; unchecked items need hardware, platforms, or data this project does not have yet.

- [x] GUI is navigable, responsive, and shows progress/cancellation (offscreen GUI tests, fuzz, screenshots).
- [x] Audio loading supports advertised formats (WAV PCM/float, extensible); long-file analysis has a bounded-window design and a result-memory cap (measured 33 MB at 1 h, 82 MB at 3 h); report serialization is still in-memory.
- [x] Mono/stereo/multichannel findings include frequency, level, time, harmonic evidence with thresholds, and qualified wording. Noise-floor/SNR evidence is limited to the local-floor prominence per measurement.
- [x] Spectrum (plot + table), waveform, spectrogram, and findings/track timeline are implemented and visually checked.
- [x] Two-recording comparison (independent summary) is implemented and reproducible.
- [x] Versioned JSON export (schema 1.2): input hash and byte length, all result-affecting settings, application/method versions, warnings (clipping, left-out tracks); schema validated in CI (`jsonschema` required). No timestamp by design (byte-identical reruns); no report digest or importer yet.
- [x] Invalid/unsupported input handling and source preservation are tested (corruption matrix, output-never-overwrites-input).
- [x] No silent resampling/alignment/downmix; recorded as not applied in comparisons.
- [ ] Keyboard-only operation is implemented and tested; screen-reader summaries/tables exist but Narrator/AT-SPI verification, focus visibility, and high-DPI checks on packaged apps are pending.
- [ ] Labeled validation corpus: synthetic layer done ([results](../validation/synthetic-results.md)); no real labeled/held-out corpus, so no field false-positive/negative claims.
- [x] Third-party notices drafted ([notices](THIRD_PARTY_NOTICES.md)); Qt LGPL review for the exact distributed build is pending.
- [ ] Linux packages build and install (.deb/.tar.gz smoke-tested in CI); Windows zip is configured but unverified; clean-VM install/upgrade tests not run.
- [x] CI: core, sanitizers, Qt build, packaging, 30 s fuzzers. Dependency review/SBOM pending.
- [x] User guide, limitations, release notes, QA report match the build.

The 50/60 Hz evidence must not be presented as proof of a ground loop, location, device, recording date, or authenticity.
