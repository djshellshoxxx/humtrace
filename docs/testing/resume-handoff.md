# HumTrace continuation handoff — 2026-10-24

## Current source and published release

- Repository: `djshellshoxxx/humtrace` only.
- Branch: `codex/humtrace-release-0.2.4`.
- Latest source implementation checkpoint to push: `7379de3751d2afed8978da0cf7f5ced0092712c4`.
- Remote branch was last verified at `889065fcd8e67fc1a3e3dc4983a6fb19a7348c54`; the pending source/docs commits should be pushed with a normal fast-forward and the final remote SHA verified.
- Pull request: [draft PR #9](https://github.com/djshellshoxxx/humtrace/pull/9), open; update its validation section after the next CI run.
- Published preview: [v0.2.5](https://github.com/djshellshoxxx/humtrace/releases/tag/v0.2.5), tag object `6ca598bcdda17384b4bb924cec2d1ec9f005c3c6`, peeled source `d9a24852024e86bff7e25d64bd50a73d02944c6c`. Preserve that release and its assets.

## Completed source and documentation work

The Windows package workflow stages exact-version MSYS2 source-only recipe
archives for the package owners of bundled runtime DLLs. It validates package
page versions, redirects, documented MSYS2 primary/Tier 1 mirror hosts, and the
final `/mingw/sources/<versioned archive>` path. It caps download sizes, records
locally computed SHA-256 values, and identifies the archives as build recipes
and patches rather than upstream source archives. The Windows ZIP includes the
staged records; CI runs fixture tests.

HumTrace's first plugin workflow is specified as live DAW capture through a
transparent effect followed by offline analysis. Both VST3 and CLAP are
selected, with REAPER 7.83 as the first host target and mono/stereo F32,
8–192 kHz, 60-second default, and 1–120-second capture bounds. The README and
acceptance docs label the project as a software artwork and audio-analysis
application. No capture job, adapter, plugin binary, or tested host support has
been implemented.

## Validation evidence and last CI failure

The 55 portable packaging tests pass locally:

- `python3 tests/test_packaging.py` — 2
- `python3 tests/test_package_licenses.py` — 12
- `python3 tests/test_msys2_sources.py` — 12
- `python3 tests/test_qt_sources.py` — 5
- `python3 tests/test_macos_packaging.py` — 14
- `python3 tests/test_package_source.py` — 6
- `python3 tests/test_source_metadata.py --cmake /workspace/humtrace-tools/cmake/data/bin/cmake` — 4

`py_compile`, `git diff --check`, and staged-patch secret-pattern checks pass.
The C++ CTest evidence remains the earlier checkpoint's 29/29 and does not test
the new Python package collector.

CI run [40105892447](https://github.com/djshellshoxxx/humtrace/actions/runs/40105892447)
finished with 5 jobs passing (Linux core, Linux sanitizers, Linux GUI, macOS
ARM64, macOS Intel) and Windows packaging failing. The Windows collector
rejected the exact redirect
`https://ftp2.osuosl.org/pub/msys2/mingw/sources/mingw-w64-brotli-1.2.1-1.src.tar.zst`
because that official Tier 1 mirror was missing from its allowlist. The fix at
source checkpoint `7379de3` adds the documented primary/Tier 1 hosts, requires
the source-path suffix and archive filename/version, and tests the redirect.
No Windows artifact was uploaded in the failed run; retry CI from the corrected
source before accepting this provenance step.

A live request from this saved executor to
`https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qt6-base` for
version `6.12.0-1` is separately blocked by its HTTP 403 CONNECT proxy. No page
or archive was downloaded locally. Do not bypass that proxy. The corrected
GitHub runner is the pending live-download check.

## Next actions and remaining gates

1. Push the local implementation and handoff commits to the existing branch
   with a normal fast-forward, verify the remote SHA, and update draft PR #9.
2. Wait for the resulting CI. On a pass, inspect the Windows package artifact,
   manifest, source recipe archives, hashes, and extracted smoke result. On a
   failure, preserve the exact job log and fix from its evidence.
3. Continue with macOS dylib-to-Homebrew formula mapping and other bounded
   provenance work. Full report import/verification, capture provenance and
   actual plugin adapters remain incomplete. VST3/CLAP have design choices but
   no implementation or host validation; standalone packages are not plugins.
4. Held-out recordings, calibrated uncertainty, platform accessibility and
   clean-machine acceptance, complete artifact-specific licensing review, and
   signing/notarization remain open. Windows is unsigned; macOS signatures are
   ad-hoc. Do not publish a successor until native artifacts, tests, notes,
   checksums and remaining gates are reviewed.
