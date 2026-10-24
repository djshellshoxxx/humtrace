# HumTrace continuation handoff — 2026-10-24

## Current source and published release

- Repository: `djshellshoxxx/humtrace` only.
- Branch: `codex/humtrace-release-0.2.4`.
- Latest implementation checkpoint to push: `bd56620312e0536d2a7b7f9141a245e3530e65ab`.
- Remote branch was last verified at `bd46e1e771fd908259799a50669032071ff5937c`; push the pending implementation/handoff commits with a normal fast-forward and verify the final remote SHA.
- Pull request: [draft PR #9](https://github.com/djshellshoxxx/humtrace/pull/9), open; update its validation section after the next CI run.
- Published preview: [v0.2.5](https://github.com/djshellshoxxx/humtrace/releases/tag/v0.2.5), tag object `6ca598bcdda17384b4bb924cec2d1ec9f005c3c6`, peeled source `d9a24852024e86bff7e25d64bd50a73d02944c6c`. Preserve that release and its assets.

## Completed source and documentation work

The Windows package workflow stages exact-version MSYS2 source-only recipe
archives for the package owners of bundled runtime DLLs. It validates package
page versions, redirects, documented MSYS2 primary/Tier 1 mirror hosts, and the
final `/mingw/sources/<versioned archive>` path. It caps per-archive and
aggregate downloads at 256 MiB and 512 MiB, records locally computed SHA-256
values, and identifies the archives as recipes and patches rather than upstream
source archives. The Windows ZIP includes the staged records; CI runs fixture
tests.

HumTrace's first plugin workflow is specified as live DAW capture through a
transparent effect followed by offline analysis. Both VST3 and CLAP are
selected, with REAPER 7.83 as the first host target and mono/stereo F32,
8–192 kHz, 60-second default, and 1–120-second capture bounds. The README and
acceptance docs label the project as a software artwork and audio-analysis
application. No capture job, adapter, plugin binary, or tested host support has
been implemented.

## Validation evidence and last CI failures

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
passed Linux core, sanitizer, GUI, and both macOS jobs, but Windows packaging
failed closed on the official `ftp2.osuosl.org` mirror redirect missing from
the initial allowlist. Checkpoint `7379de3` added the official primary/Tier 1
host list and path validation. The next CI run
[40106555687](https://github.com/djshellshoxxx/humtrace/actions/runs/40106555687)
passed the mirror check but failed because
`mingw-w64-gcc-16.2.0-6.src.tar.zst` exceeded the original 64 MiB per-archive
limit. Its other five jobs passed. Checkpoint `bd56620` raises the per-archive
cap to 256 MiB and the aggregate cap to 512 MiB. No Windows artifact was
uploaded in either failed run; retry from this checkpoint before accepting the
provenance step.

A live HEAD request from this executor to
`https://ftp2.osuosl.org/pub/msys2/mingw/sources/mingw-w64-gcc-16.2.0-6.src.tar.zst`
is blocked by its HTTP 403 CONNECT proxy. No source archive was downloaded or
staged locally. Do not bypass the proxy; the corrected GitHub runner is the
pending live-download check.

## Next actions and remaining gates

1. Push checkpoint `bd56620` and this handoff commit to the existing branch
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
