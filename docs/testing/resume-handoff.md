# HumTrace continuation handoff — 2026-10-24

## Current source and published release

- Repository: `djshellshoxxx/humtrace` only.
- Branch: `codex/humtrace-release-0.2.4`.
- Latest implementation checkpoint: `9ee9d35` (macOS Homebrew provenance candidate inventory); the following handoff commit is documentation-only. Verify the exact remote ref after pushing both commits with a normal fast-forward.
- Pull request: [draft PR #9](https://github.com/djshellshoxxx/humtrace/pull/9), open and mergeable; update its body after publishing the checkpoint and observing its CI.
- Published preview: [v0.2.5](https://github.com/djshellshoxxx/humtrace/releases/tag/v0.2.5), tag object `6ca598bcdda17384b4bb924cec2d1ec9f005c3c6`, peeled source `d9a24852024e86bff7e25d64bd50a73d02944c6c`. Preserve that release and its assets.

## Completed source and documentation work

The Windows package workflow stages exact-version MSYS2 source-only recipe
archives for the package owners of bundled runtime DLLs. It validates package
page versions, redirects, documented MSYS2 primary/Tier 1 mirror hosts, and the
final `/mingw/sources/<versioned archive>` path. It caps per-archive and
aggregate downloads at 256 MiB and 512 MiB, records locally computed SHA-256
values, and identifies the archives as recipes and patches rather than upstream
source archives. CI run 40107153073 passed the collection and extracted-package
smoke test: 21 recipes were staged for 22 runtime owners.

The current macOS packaging change maps each packaged Mach-O image by exact
filename against installed Homebrew Cellar kegs. It records unique-formula,
ambiguous, and unmapped cases, formula license/source metadata, matching keg
file hashes under 2 GiB aggregate and 512 MiB per-file caps, and installed
formula recipes when available. It also includes the exact installed
`brew info --json=v1 --installed` inventory. This is informational candidate
provenance, not proof of binary correspondence, complete source, license
applicability, or legal compliance. Seven fixture tests pass. Native CI must
confirm the macOS runner metadata layout.

HumTrace's first plugin workflow is specified as live DAW capture through a
transparent effect followed by offline analysis. Both VST3 and CLAP are
selected, with REAPER 7.83 as the first host target and mono/stereo F32,
8–192 kHz, 60-second default, and 1–120-second capture bounds. The README and
acceptance docs label the project as a software artwork and audio-analysis
application. No capture job, adapter, plugin binary, or tested host support has
been implemented.

## Validation evidence and CI history

The 62 portable packaging/provenance tests pass locally:

- `python3 tests/test_packaging.py` — 2
- `python3 tests/test_package_licenses.py` — 12
- `python3 tests/test_msys2_sources.py` — 12
- `python3 tests/test_qt_sources.py` — 5
- `python3 tests/test_macos_packaging.py` — 14
- `python3 tests/test_macos_provenance.py` — 7
- `python3 tests/test_package_source.py` — 6
- `python3 tests/test_source_metadata.py --cmake /workspace/humtrace-tools/cmake/data/bin/cmake` — 4

Python compilation, `git diff --check`, and staged-patch secret-pattern checks
pass. The C++ CTest evidence remains the earlier checkpoint's 29/29 and was not
rerun because this slice changes packaging metadata only.

CI run
[40107153073](https://github.com/djshellshoxxx/humtrace/actions/runs/40107153073)
passed all six jobs on implementation checkpoint
`9456747194c858bee900566ff525febeb2c05728`: Linux core/CLI QA, sanitizer,
GUI/memory/fuzz, Windows package, macOS 15 Intel, and macOS 26 ARM64. Windows
passed build/tests, package, and extracted ZIP smoke (isolated PATH, UTF-8,
CLI analysis, GUI initialization, five-second event loop); it staged 21 MSYS2
source-only recipe archives for 22 owners. Its nested Windows package SHA-256
from the job log is
`44260dcff2fc57dfbac1591ad893947bd2715fc0c4440e519444288beceadb5f`.
The outer GitHub artifact SHA-256 is
`20a160d4e5e86cfd42b616bda313512840e477812efe7e7639eb1c7f6f1aa2c1`.
Other outer artifact digests: Linux
`1fb2f4066fb9b6279509b68433b75c1165fd15538a62d76663549c915ffbf695`,
macOS x86_64
`5ceec3d3e6044f569c24406ea4131eef62093af813abf846c8a9e333d02917e6`,
macOS ARM64
`ec8f6d18a47c5ebffd2ee735e0beb06d91c34cd8bcec4eb10a09fb1dbfed547e`.
These outer digests are not the nested macOS/Linux package checksums. A
separate local byte-level audit of artifact contents remains pending because
the artifact download connector supplies a file reference without a
workspace-readable path.

A live HEAD request from this executor to the MSYS2 GCC recipe URL is blocked by
its HTTP 403 CONNECT proxy. The corrected native Windows runner performed the
download and staged the recipe archives. Continue respecting the proxy; no
bypass was attempted.

## Next actions and remaining gates

1. Push the implementation and this handoff to the existing branch with a
   normal fast-forward, verify the exact remote head, and update draft PR #9.
2. Check the resulting CI for both macOS jobs. Confirm the provenance JSON
   includes expected formula candidates and that ambiguous/unmapped results
   remain visible; adjust only from native-runner evidence.
3. Perform byte-level audit of the Windows/macOS/Linux artifacts when a safe
   workspace-readable transfer is available; distinguish outer GitHub
   artifact digests from nested package checksums.
4. Continue bounded report import/verification, capture provenance, and
   licensing/provenance work. Actual VST3/CLAP adapters and host validation
   remain incomplete; standalone packages are not plugins.
5. Held-out recordings, calibrated uncertainty, platform accessibility and
   clean-machine acceptance, complete artifact-specific licensing review, and
   signing/notarization remain open. Windows is unsigned; macOS signatures are
   ad-hoc. Do not publish a successor until native artifacts, tests, notes,
   checksums and remaining gates are reviewed.
