# Development status — 2026-10-22

Active development branch: `claude/project-completion-builds-sokivz` (PR #6). This branch contains the Qt desktop application and is substantially ahead of `main` and the newer CLI-only branches. No other branches have been deleted or overwritten.

## Completed in this continuation

- Ported `feat/welch-psd` into the existing desktop analysis pipeline, preserving tracker, comparison, cancellation, source hashing, atomic output and GUI modules.
- Added streaming per-channel PSD with complete-segment accounting, median floor and nominal harmonic prominence; reports record all estimator settings and method version. CLI segment/hop controls apply to analysis and comparison.
- Fixed PSD false evidence from coarse resolution or an absent noise-floor neighborhood; reject power overflow.
- Added raw per-frame mean/DC, RMS and sample peak, with stable scaled arithmetic and explicit numeric-floor flags; report schema, CLI integration tests and desktop inspector updated.
- Separated Qt-independent presentation text into a library so CLI-only CMake builds run its tests.
- Desktop summaries remain bounded for multichannel files, with per-channel PSD detail in the inspector.
- Make builds reuse core objects across test executables instead of recompiling every source for every test.

## Verification

The current CMake CLI-only build passes all 16 tests, including headless presentation and mandatory JSON schema validation. A fresh optimized Make build also passes all core and CLI tests. Raw-level tests passed AddressSanitizer/UndefinedBehaviorSanitizer with leak detection disabled because this container prevents LeakSanitizer reading process information. Full leak/sanitizer coverage remains a CI check.

Qt is unavailable locally; installation was blocked by the environment's network policy. The initial PSD commit passed CI's Linux GUI build, GUI tests/fuzzing, packaging and installed-package smoke test. Later changes require their own CI results; these are not implied by the earlier pass. Native Windows/macOS checks and packaged GUI accessibility remain release gates.

## Remaining work

The product is not fully complete. Existing specifications still include optional ENF, diagnostic guidance, signature library, plugin adapter, playback and non-WAV decoding. Analysis-engine gaps include configurable detrending/frequency bands, calibrated uncertainty, richer harmonic grouping and track split/merge events. Report results remain in memory behind the configured cap. A labeled held-out real-recording corpus, clean-machine platform testing, screen-reader/high-DPI checks and exact distributed Qt license/SBOM review are outstanding. See product requirements and beta checklist for the applicable first-release gates; do not describe deferred specs as implemented features.
