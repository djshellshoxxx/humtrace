# Spec status (docs/specs/product-requirements.md, docs/release/beta-checklist.md)

DONE = implemented and tested here; PARTIAL = some of it; MISSING = not started.

| Requirement | Status | Evidence / gap |
|---|---|---|
| Load PCM/float WAV | DONE | `src/wav.cpp`, `tests/test_wav.cpp`, `tests/fuzz_wav.cpp`. AIFF/FLAC/MP3 deferred by spec ("later"). |
| Mono/stereo independent, bounded memory | DONE | `src/analysis.cpp` streams windows; result cap `max_result_frames`; 10-min stereo run peaked ~11 MB RSS. |
| Overview metadata | DONE | CLI header, GUI summary, JSON `input`. |
| Waveform view | MISSING | Not implemented. |
| Spectrogram view | MISSING | Not implemented. |
| Spectrum view | DONE | `gui/timeline_widget.cpp` `SpectrumWidget` (selected frame). Not visually reviewed by a human. |
| Finding timeline | PARTIAL | Per-frame harmonic-support bars + table. No persistent tracks/intervals. |
| Persistent narrowband tone detection | PARTIAL | Per-frame strongest peak only; no cross-frame association. |
| 50/60 Hz harmonic candidates w/ level, local floor, prominence | DONE (per frame) | `measure_harmonics`; persistence/evidence aggregation only in comparison summary. |
| Exposed settings, versioned reproducible report | DONE | CLI `--frame-size/--threshold/--prominence`, GUI controls, schema 1.1. |
| Input hashes, build metadata, warnings in report | DONE | `input.sha256`, `size_bytes`, `application_version`, `warnings` (`tests/test_report.cpp`, `test_cli.py`). |
| Invalid files, cancellation, source preserved | DONE | `tests/test_core.cpp`, `test_cli.py`, GUI smoke. Cancel is unit-tested in core and wired to Ctrl-C/GUI Cancel; GUI cancel click itself is not UI-tested. |
| Progress + cancel off the UI thread | DONE | `gui/main_window.cpp` worker thread. |
| Multi-file comparison | PARTIAL | `humtrace --compare` text summary; no GUI, no JSON. |
| No silent resample/downmix | DONE | Channels analyzed independently; nothing resampled. |
| Keyboard-only/screen-reader/contrast/high-DPI | PARTIAL | Accessible names, table equivalent, arrow-key timeline navigation. NOT tested with a screen reader or on high-DPI. |
| Labeled validation corpus (FP/FN) | MISSING | Needs real recordings; only synthetic tests exist. |
| Linux package | DONE (unsigned tarball) | `scripts/package_linux.sh`, smoke-tested. Qt not bundled (LGPL review). |
| Windows package | PARTIAL | CLI zip built by CI only; GUI not built or run on Windows. |
| CI / sanitizers / releases | DONE | `.github/workflows/`. TSan not used (core has no threads; GUI worker is exercised by smoke test only). |
| Dependency/license review | PARTIAL | `THIRD-PARTY-NOTICES`; Qt LGPL decision flagged to owner. |
| Docs match build | DONE | README, limitations, schema doc, checklist updated. |
