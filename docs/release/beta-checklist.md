# Beta v0.1 release checklist

Current status: **feature-complete beta candidate; not release-ready**. A checked item describes the recorded CI or repository evidence stated in that item; it does not by itself establish a releasable artifact or a support claim for every platform.

- [ ] The v0.2.4 native CI run builds and tests the Qt GUI and attempts package smoke checks on Linux, Windows, Apple silicon macOS, and Intel macOS. Windows packaging is awaiting rerun after a byte-stable test-fixture fix; macOS dependency closure and ad-hoc signature verification are still in progress. Even passing runner checks do not verify keyboard-only or assistive-technology use, interactive native UI, clean-machine installation, or notarization.
- [x] Audio loading supports the advertised WAV PCM/float and extensible encodings. Long-file analysis uses bounded streaming and disk-backed frame/track storage with a result-memory cap (recorded measurements: 33 MB at 1 h, 82 MB at 3 h). JSON export is incremental and cancellable; report import/verification is not implemented.
- [x] Mono/stereo/multichannel findings include frequency, level, time, harmonic evidence with thresholds, and qualified wording. Noise-floor/SNR evidence is limited to the local-floor prominence per measurement.
- [x] Spectrum (plot + table), waveform, spectrogram, and findings/track timeline are implemented and visually checked.
- [x] Two-recording comparison (independent summary) is implemented and reproducible.
- [x] Versioned JSON export (schema 1.3): input hash and byte length, effective detector and Welch settings, peak resolution/quality metadata, tracking statistics and candidate ambiguity references; schema validation is required in CI. No timestamp by design (byte-identical reruns); no report digest or importer yet.
- [x] Invalid/unsupported input handling and source preservation are tested (corruption matrix, output-never-overwrites-input).
- [x] No silent resampling/alignment/downmix; recorded as not applied in comparisons.
- [ ] Keyboard-only operation, visible focus, and screen-reader summaries/tables are verified in native user workflows. Narrator/AT-SPI checks, packaged-app focus and high-DPI checks, and accessibility acceptance remain pending.
- [ ] Labeled validation corpus: synthetic layer done ([results](../validation/synthetic-results.md)); no real labeled/held-out corpus, so no field false-positive/negative claims.
- [ ] Third-party notices and exact redistribution obligations are complete. The notice draft exists ([notices](THIRD_PARTY_NOTICES.md)). The Windows helper inventories package license files for bundled root runtime DLLs; the Qt source helper stages a checksum-verified upstream QtBase source baseline and its `LICENSES` tree. Neither establishes all bundled modules/plugins, distribution-patched corresponding sources, non-Qt dependencies, applicable license options, relinkability, or legal compliance. The project has no product `LICENSE` file specifying an authored-code license.
- [ ] The v0.2.4 native package run has verified artifacts for Ubuntu 24.04, Windows UCRT64, Apple silicon macOS, and Intel macOS. The workflow is configured for these builds and package smoke tests, but current Windows and macOS packaging runs are pending. Linux `.tar.gz` is built but not installed.
- [ ] Release package runtime closure and user-workflow verification are complete. The v0.2.3 Windows archive lacked root Qt DLLs, and its runner `PATH` masked that defect. A corrected Windows package must pass isolated smoke; macOS still needs dependency closure/signature results. Clean-machine install/upgrade tests are not run.
- [ ] Latest native CI: Linux core/sanitizer and GUI jobs plus Windows and both macOS package jobs must pass for the current v0.2.4 source. The current Windows fixture fix and macOS packaging work need native reruns. Dependency SBOM and complete package dependency/source review remain pending.
- [x] User guide, limitations, release notes, QA report match the build.

The 50/60 Hz evidence must not be presented as proof of a ground loop, location, device, recording date, or authenticity.
