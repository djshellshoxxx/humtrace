# HumTrace 0.2.4 development preview

This preview adds reproducible detector controls in the desktop and CLI: explicit
frame/hop, independent spectrum/Welch detrending, peak band/separation, baseline
neighborhoods, thresholds and bounded nominal-harmonic search. Schema 1.3 exports
effective choices, raw/refined frequency, resolution/width and validity metadata.
Calibrated frequency uncertainty remains uncharacterized and is exported as null.

Tracking now ends ambiguous candidate associations and records split/merge/competing
events with fresh segment IDs. The same graph relationships, quantiles, drift fit
quality and gaps survive retained, paged and streamed exports, including suppressed
short segments. These relationships do not establish physical source identity.

Long/dense ordinary CLI runs now page to private disk storage, as the desktop does.
This fixes the final QA memory-growth failure without changing its acceptance gate.
JSON publication remains bounded, cancellable and atomic. A second input hash detects
persistent same-size edits with restored timestamps. Comparisons retain both inputs'
settings; extreme pitch labels, CSV unavailable values and locale-independent numeric
encoding have regression coverage. Desktop fixes cover fractional thresholds,
measurement tables, stale text, separate harmonic-family counts and late cancellation.

Windows packaging now resolves the complete runtime DLL dependency closure and tests
the extracted executables with the toolchain removed from PATH. This corrects the
missing root runtime DLLs in v0.2.3. Qt packages include license/source inventories;
these record scope and do not certify all licensing obligations. Each release includes
checksums, a source archive and SOURCE_COMMIT.txt. See the exact-tag native CI run
before treating an archive as available.

## Installation

- Linux: Ubuntu 24.04 runner baseline; install the `.deb` with the system package
  manager so Qt dependencies are resolved, or extract the `.tar.gz` with compatible
  system Qt 6 libraries installed.
- Windows: extract the complete ZIP and keep all DLLs and plugin subdirectories
  beside `humtrace.exe` and `humtrace-gui.exe`. The build is unsigned.
- macOS: choose the ARM64 or x86_64 archive, extract it and open `humtrace-gui.app`.
  The deployment target is macOS 15.0. The app is ad-hoc signed and not notarized;
  follow the OS's explicit first-launch approval flow. The archive also includes CLI.

The [acceptance ledger](acceptance-0.2.4.md) keeps the remaining gates explicit:
held-out labeled real-data calibration, native accessibility/clean-machine acceptance,
full provenance/import, artifact-specific licensing review and signing/notarization.
Only WAV PCM/float/extensible input is advertised. Similar hum frequencies do not
prove a device, origin, location, authenticity or recording time.

VST3 and CLAP are not included. HumTrace is an offline measurement application;
the [bounded capture proposal](../specs/plugin-capture-interface.md) requires a
host/capture workflow decision and separate SDK/host verification before adapters
can be implemented and published. Standalone archives are not plugin files.
