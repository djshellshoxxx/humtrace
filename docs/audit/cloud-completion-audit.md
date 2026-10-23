# Cloud completion audit

The main-line prototype was missing substantial implementation already present in
`claude/project-completion-builds-sokivz`. That branch is integrated here with its
history preserved. Its earlier QA statements describe earlier runs, not evidence
that this merge or a new platform package has passed.

## Implemented development workflow

The dependency-free C++20 core and CLI include streaming WAV decoding, per-channel
FFT/peak/harmonic measurements, Welch PSD, raw sample levels, descriptive peak
tracks, independent-recording comparison, SHA-256 input integrity metadata,
versioned JSON and track CSV exports. The Qt desktop includes background jobs,
progress/cancel, waveform/spectrum/spectrogram views, measurement tables and
finding inspection.

The CI workflow configures native Linux (Ubuntu 24.04), Windows UCRT64, Apple
silicon macOS, and Intel macOS jobs. Linux CI builds `.deb`/`.tar.gz`, installs
the `.deb`, runs the CLI on generated audio, and attempts a timed installed GUI
launch. Windows builds and offscreen-tests the GUI, creates a ZIP, runs the
packaged CLI on generated audio, and launches the extracted packaged GUI for five
seconds using the native Windows plugin. Both macOS architectures build and
offscreen-test the GUI, create an ad-hoc-signed app bundle archive, and run the
packaged CLI plus GUI for five seconds with Qt offscreen mode. Linux attempts a
timed GUI launch after installing the `.deb`. These are runner CI checks, not
clean-machine install, upgrade, screen-reader, interactive native UI, or native
user-workflow acceptance; macOS signing remains ad hoc and is not notarized.

This revision adds export collision rejection, exclusive temporary creation,
no-replace publication without `--force`, Windows replacement support, UTF-8 path
regressions, Qt 6.8-compatible bounded GUI test waits, and optional-view allocation
avoidance for the CLI. Report equality tests protect numerical parity between
presentation-enabled and CLI analysis.

The v0.2.3 Windows ZIP was subsequently inspected and found to contain the
executables and Qt plugin folders without the root Qt DLLs. The runner's
`PATH` supplied missing runtime dependencies during its smoke test, so that
test did not establish package runtime closure. A corrected artifact must pass
an isolated package smoke test before the Windows package gate can close.

## Long-file implementation and uncompleted requirements

- Long-file desktop analyses now use immutable disk-backed frame/track storage,
  exact random-access inspector records, a byte-budgeted LRU and multiresolution
  spectrogram viewport tiles. Small-file in-memory results retain a caller-visible
  cap. Comparison loads bounded track summaries in a cancellable background task;
  oversized track/candidate workloads fail explicitly rather than discard data.
  JSON export is incremental and cancellable. The CLI and desktop hour-scale
  acceptances independently measure actual working set and source immutability.
- Track split/merge lineage, full event intervals, fit-quality/quantile output and
  independently validated persistence rules remain incomplete.
- A labeled recording corpus split by source/session, field performance and
  forensic accuracy claims are unavailable. Synthetic or unlabeled real-file
  checks do not satisfy this gate.
- Packaged Narrator/AT-SPI, high-DPI, keyboard focus, clean-machine install/upgrade
  and uninstall tests require their own platform evidence. Windows/macOS package
  GUI checks only exercise startup and a short event loop; macOS uses offscreen
  Qt, and Windows/Linux event handling is not an interactive user acceptance
  test.
- Report import and independent verification workflow, dependency SBOM, exact
  per-artifact Qt version/module/plugin inventory, associated license/source/
  relinking materials, and compliance review remain outstanding. The root
  source has no product `LICENSE` file, so this audit selects no HumTrace code
  license.
- ENF matching, signature libraries, hardware diagnostic guidance and plugin
  adapters are separately deferred specs. Standalone archives are not plugins.
  The planned Windows/macOS/Linux standalone platform families define the
  intended plugin platform scope too, but no VST3 or CLAP format, host, or
  plugin platform is currently supported or validated. See
  `../specs/plugin-adapter.md` and `../specs/plugin-capture-interface.md`.

A native preview release may expose the implemented functionality with these
limitations, but does not close the full beta acceptance checklist. Passing a
memory ratio or a native build alone must not be described as completing these
requirements. See `release/beta-checklist.md` and each detailed specification.
