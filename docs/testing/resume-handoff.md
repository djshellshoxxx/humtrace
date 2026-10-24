# HumTrace continuation handoff — 2026-10-24

## Current source and published release

- Repository: `djshellshoxxx/humtrace` only.
- Branch: `codex/humtrace-release-0.2.4`.
- Latest implementation checkpoint: `9456747194c858bee900566ff525febeb2c05728`.
- Remote branch was verified at the same SHA with `git ls-remote`.
- Pull request: [draft PR #9](https://github.com/djshellshoxxx/humtrace/pull/9), open and mergeable; CI run 40107153073 passed on this checkpoint.
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

## Validation evidence and CI history

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

CI runs `40105892447` and `40106555687` exposed, respectively, an official
MSYS2 Tier 1 mirror redirect and a GCC source recipe exceeding the initial
64 MiB cap. Checkpoints `7379de3` and `bd56620` addressed those cases by
validating the published primary/Tier 1 host list and allowing 256 MiB per
archive / 512 MiB aggregate. The follow-up run
[40107153073](https://github.com/djshellshoxxx/humtrace/actions/runs/40107153073)
passed all six jobs: Linux core/CLI QA, sanitizer, GUI/memory/fuzz, Windows
package, macOS 15 Intel, and macOS 26 ARM64. Windows staged 21 MSYS2
source-only recipe archives for 22 runtime owners, built the package, and
passed the extracted ZIP smoke test (isolated PATH, UTF-8 input, CLI analysis,
GUI initialization and a five-second event loop). The four uploaded artifacts
are `linux-packages` (1,147,688 bytes; SHA-256
`1fb2f4066fb9b6279509b68433b75c1165fd15538a62d76663549c915ffbf695`),
`windows-zip` (404,737,218 bytes; SHA-256
`20a160d4e5e86cfd42b616bda313512840e477812efe7e7639eb1c7f6f1aa2c1`),
`macos-x86_64-zip` (78,680,485 bytes; SHA-256
`5ceec3d3e6044f569c24406ea4131eef62093af813abf846c8a9e333d02917e6`), and
`macos-arm64-zip` (79,547,276 bytes; SHA-256
`ec8f6d18a47c5ebffd2ee735e0beb06d91c34cd8bcec4eb10a09fb1dbfed547e`). These
are GitHub Actions artifact archive digests, not the nested platform ZIP
checksums. The generated artifact packages passed runner smoke tests; a
separate local byte-level audit of their contents is still pending.

A live HEAD request from this executor to
`https://ftp2.osuosl.org/pub/msys2/mingw/sources/mingw-w64-gcc-16.2.0-6.src.tar.zst`
is blocked by its HTTP 403 CONNECT proxy. The corrected native Windows runner
performed the download and staged the recipe archives. Continue respecting
the proxy; no bypass was attempted.

## Next actions and remaining gates

1. Perform a byte-level audit of the Windows artifact and other platform
   packages when a safe workspace-readable artifact transfer is available;
   distinguish GitHub artifact digests from nested package checksums.
2. Continue with macOS dylib-to-Homebrew formula mapping and bounded
   provenance/licensing work. Full report import/verification, capture
   provenance and actual plugin adapters remain incomplete. VST3/CLAP have
   design choices but no implementation or host validation; standalone
   packages are not plugins.
3. Held-out recordings, calibrated uncertainty, platform accessibility and
   clean-machine acceptance, complete artifact-specific licensing review, and
   signing/notarization remain open. Windows is unsigned; macOS signatures are
   ad-hoc. Do not publish a successor until native artifacts, tests, notes,
   checksums and remaining gates are reviewed.
