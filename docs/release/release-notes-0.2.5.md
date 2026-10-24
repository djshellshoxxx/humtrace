# HumTrace 0.2.5 development preview

This patch fixes Windows package source-revision metadata. The v0.2.4 Windows
archive ran successfully on its native runner, but its embedded
`SOURCE_COMMIT.txt` said `unknown` because CMake could not find Git in the MSYS2
build environment. The release-level source SHA and exact-tag CI trace were
correct. Existing v0.2.4 tags and assets are preserved.

Native CI now supplies the checkout revision explicitly to CMake. The value must
be a full hexadecimal Git revision; extracted or installed package smoke tests require the
embedded source file to match that checkout before launching either executable.
Regressions cover CRLF metadata, absent/unknown/mismatched revisions, invalid IDs
and bounded reads. Detector, tracking and report behavior are unchanged from
the [0.2.4 preview](https://github.com/djshellshoxxx/humtrace/releases/tag/v0.2.4).

## Installation and scope

- Linux: Ubuntu 24.04 baseline; install the `.deb` with a package manager, or
  extract the `.tar.gz` with compatible system Qt 6 libraries available.
- Windows x64: extract the entire ZIP, preserving its DLLs and plugin folders.
  The executables are unsigned.
- macOS: use the ARM64 archive on macOS 26 or the Intel archive on macOS 15.
  Bundled images are checked against those baselines. Signatures are ad-hoc;
  the app is not notarized. The archive includes the standalone CLI.

All packages are standalone applications. VST3/CLAP adapters remain a
[capture/host interface proposal](https://github.com/djshellshoxxx/humtrace/blob/v0.2.5/docs/specs/plugin-capture-interface.md),
pending a workflow, target-host and capture-limit decision.

The [acceptance ledger](https://github.com/djshellshoxxx/humtrace/blob/v0.2.5/docs/release/acceptance-0.2.4.md)
still applies: held-out detector calibration, native human accessibility and
clean-machine acceptance, full provenance/import, artifact-specific licensing
review and signing/notarization remain open. Qt source and notice inventories
record their scope; they do not certify licensing compliance. Hum measurements
do not prove source identity, authenticity, location or recording time.

The release includes a source archive, `SOURCE_COMMIT.txt` and SHA-256 checksums.
Its appended exact-tag CI link identifies the native build and package evidence.
