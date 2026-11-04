# Spec status (docs/specs/product-requirements.md, docs/release/beta-checklist.md)

DONE = implemented and tested here; PARTIAL = some of it; MISSING = not started.

| Requirement | Status | Evidence / gap |
|---|---|---|
| Load PCM/float WAV | DONE | `src/wav.cpp`, `tests/test_wav.cpp`, `tests/fuzz_wav.cpp`. AIFF/FLAC/MP3 deferred by spec ("later"). |
| Mono/stereo independent, bounded memory | DONE | `src/analysis.cpp` streams windows; result cap `max_result_frames`; 10-min stereo run peaked ~11 MB RSS. |
| Overview metadata | DONE | CLI header, GUI summary, JSON `input`. |
| Waveform view | DONE (not human visually reviewed) | GUI waveform view; verified by build/smoke test only. |
| Spectrogram view | DONE (not human visually reviewed) | GUI log-frequency spectrogram with track overlay; verified by build/smoke test only. |
| Spectrum view | DONE | `gui/timeline_widget.cpp` `SpectrumWidget` (selected frame). Not visually reviewed by a human. |
| Finding timeline | DONE (not human visually reviewed) | Per-frame harmonic-support bars + table, plus tone tracks as intervals (tracks table, spectrogram overlay). |
| Persistent narrowband tone detection | DONE (with limits) | `src/tracking.cpp`: greedy nearest-frequency association, tolerance 1.5 bins, gap <= 2 frames, min 5 frames and 0.5 s (50% frame overlap correlates adjacent frames; white-noise negative test at 256/1024/4096 frames, 5 seeds), <= 64 active / 4096 tracks (`truncated` flag when hit). Not validated on real recordings; greedy matching can swap or split close tones. `near_mains_multiple` (n x 50/60 Hz, n <= 10) is proximity only. |
| 50/60 Hz harmonic candidates w/ level, local floor, prominence | DONE (per frame) | `measure_harmonics`; persistence/evidence aggregation only in comparison summary. |
| Exposed settings, versioned reproducible report | DONE | CLI `--frame-size/--threshold/--prominence`, GUI controls, schema 1.2 (adds `tone_tracking`, `tracks[]`); `schema/comparison-report-v1.schema.json`. Schemas checked against real CLI output with python jsonschema. |
| Input hashes, build metadata, warnings in report | DONE | `input.sha256`, `size_bytes`, `application_version`, `warnings` (`tests/test_report.cpp`, `test_cli.py`). |
| Invalid files, cancellation, source preserved | DONE | `tests/test_core.cpp`, `test_cli.py`, GUI smoke. Cancel is unit-tested in core and wired to Ctrl-C/GUI Cancel; GUI cancel click itself is not UI-tested. |
| Progress + cancel off the UI thread | DONE | `gui/main_window.cpp` worker thread. |
| Multi-file comparison | DONE | `humtrace --compare` text + `--json` (comparison schema 1.0) and GUI compare dialog with JSON export. Shared tones are frequency matches, not source evidence. GUI part not human reviewed. |
| No silent resample/downmix | DONE | Channels analyzed independently; nothing resampled. |
| Keyboard-only/screen-reader/contrast/high-DPI | PARTIAL | Accessible names, table equivalent, arrow-key timeline navigation. NOT tested with a screen reader or on high-DPI. |
| Labeled validation corpus (FP/FN) | MISSING | Needs real recordings; only synthetic tests exist. |
| Linux package | DONE (unsigned tarball) | `scripts/package_linux.sh`, smoke-tested. Qt not bundled (LGPL review). |
| Windows package | PARTIAL | CI/release `windows` job (CLI + GUI via cmake, ctest offscreen, windeployqt zip) is defined but NOT run; the windeployqt tool name in msys2 is unverified. Earlier CLI-only zip did pass CI. |
| CI / sanitizers / releases | DONE | `.github/workflows/`. TSan not used (core has no threads; GUI worker is exercised by smoke test only). |
| Dependency/license review | PARTIAL | `THIRD-PARTY-NOTICES`; Qt LGPL decision flagged to owner. |
| Docs match build | DONE (updated for 1.2) | README, limitations, schema doc, checklist updated. |
