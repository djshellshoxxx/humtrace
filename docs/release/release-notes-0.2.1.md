# HumTrace 0.2.1 preview

A development preview of the desktop and CLI measurement tools. This is not a
forensic validation or source-attribution release. Deferred plugin adapters are
not included: these packages are standalone applications, not VST3/CLAP plugins.

Changes since 0.2.0:

- Skip waveform/spectrogram allocation for CLI analyses while preserving identical
  measurements. Retained result size is still capped before analysis; fully
  incremental long-file reporting remains incomplete.
- Refuse colliding JSON/CSV destinations, including hard-link aliases, before
  analysis so one export cannot silently replace another.
- Publish without replacement when `--force` is absent, including when another
  process creates the destination after preflight.
- Create temporary export files exclusively and replace existing reports with
  the native Windows operation when `--force` or a confirmed GUI save is used.
- Exercise UTF-8 paths without a locale-dependent test conversion on Windows.
- Fail desktop configuration when Qt is unavailable; explicitly select
  `-DHUMTRACE_BUILD_GUI=OFF` for the independently buildable CLI.
- Build macOS packages separately on Apple silicon and Intel native runners.
- Integrate the existing desktop/analysis implementation into the main lineage
  while retaining qualified interpretation and platform-readiness statements.

Artifacts are unsigned on Windows and ad-hoc signed, not notarized, on macOS.
Linux `.deb` targets Ubuntu 24.04; the `.tar.gz` requires a compatible Qt 6
installation. Native workflow outcomes apply only to the exact release commit.

Known limitations remain: no held-out labeled field corpus, no verified
screen-reader/high-DPI behavior on packaged apps, no clean-machine GUI upgrade
matrix, and no forensic chain-of-custody or physical-source identification.
See [beta checklist](beta-checklist.md) and [limitations](../user-guide/limitations.md).
