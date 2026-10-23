# Beta v0.1 release checklist

Current status: **feature-complete beta candidate; not release-ready**. A checked item describes the recorded CI or repository evidence stated in that item; it does not by itself establish a releasable artifact or a support claim for every platform.

- [x] Qt GUI builds and its automated offscreen GUI tests and fuzz smoke run on Linux, Windows, and macOS CI. Linux CI installs the `.deb` and attempts a timed GUI launch on a generated WAV; Windows and macOS CI launch the extracted package GUI for a five-second smoke interval, with macOS forced to Qt offscreen mode. These checks do not verify keyboard-only or assistive-technology use, interactive native UI, clean-machine installation, or macOS Gatekeeper/notarization behavior.
- [x] Audio loading supports the advertised WAV PCM/float and extensible encodings. Long-file analysis uses bounded streaming and disk-backed frame/track storage with a result-memory cap (recorded measurements: 33 MB at 1 h, 82 MB at 3 h). JSON export is incremental and cancellable; report import/verification is not implemented.
- [x] Mono/stereo/multichannel findings include frequency, level, time, harmonic evidence with thresholds, and qualified wording. Noise-floor/SNR evidence is limited to the local-floor prominence per measurement.
- [x] Spectrum (plot + table), waveform, spectrogram, and findings/track timeline are implemented and visually checked.
- [x] Two-recording comparison (independent summary) is implemented and reproducible.
- [x] Versioned JSON export (schema 1.2): input hash and byte length, all result-affecting settings, application/method versions, warnings (clipping, left-out tracks); schema validated in CI (`jsonschema` required). No timestamp by design (byte-identical reruns); no report digest or importer yet.
- [x] Invalid/unsupported input handling and source preservation are tested (corruption matrix, output-never-overwrites-input).
- [x] No silent resampling/alignment/downmix; recorded as not applied in comparisons.
- [ ] Keyboard-only operation, visible focus, and screen-reader summaries/tables are verified in native user workflows. Narrator/AT-SPI checks, packaged-app focus and high-DPI checks, and accessibility acceptance remain pending.
- [ ] Labeled validation corpus: synthetic layer done ([results](../validation/synthetic-results.md)); no real labeled/held-out corpus, so no field false-positive/negative claims.
- [ ] Third-party notices and exact redistribution obligations are complete. The notice draft exists ([notices](THIRD_PARTY_NOTICES.md)); the exact bundled Qt modules, plugins, and versions are not mapped to their license files. The macOS archive copies Qt license files, while the Windows workflow copies no Qt license directory and Linux has no per-module license inventory. Required source/relink materials remain unaudited, and the project has no product `LICENSE` file specifying an authored-code license.
- [x] Native CI builds create artifacts for Ubuntu 24.04, Windows UCRT64, Apple silicon macOS, and Intel macOS. Linux CI installs the `.deb`, runs CLI analysis, and attempts an installed GUI launch; `.tar.gz` is built but not installed. Windows and macOS archives are extracted and the packaged CLI analyzes generated audio; CI also launches the packaged GUI for five seconds (macOS with Qt offscreen mode).
- [ ] Release package runtime closure and user-workflow verification are complete. The v0.2.3 Windows archive was found to contain executables and Qt plugin folders without root Qt DLLs; its runner `PATH` masked the missing runtime dependency closure. A corrected Windows package still needs an isolated package smoke test. Clean-machine install/upgrade tests are not run.
- [x] CI: Linux core, sanitizers, Qt builds/tests and packaging across the configured native runners, with 30 s fuzzers. Dependency review/SBOM and exact package dependency closure remain pending.
- [x] User guide, limitations, release notes, QA report match the build.

The 50/60 Hz evidence must not be presented as proof of a ground loop, location, device, recording date, or authenticity.
