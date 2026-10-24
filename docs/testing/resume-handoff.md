# HumTrace continuation handoff — 2026-10-24

## Repository and checkpoints

- Work only in `djshellshoxxx/humtrace`, branch `codex/humtrace-release-0.2.4`.
- Latest implementation checkpoint: `bec24d7086a212b4b4b82640a5f53d480d34f90f` adds a bounded source-tree report verifier; the branch was pushed and its remote SHA verified.
- Earlier full native CI run [40108498575](https://github.com/djshellshoxxx/humtrace/actions/runs/40108498575) passed on branch head `de6591bd8a55f0afc1822ccf2646672801a41f45`. It predates the report-verifier change and does not validate that code.
- Handoff-only CI [40109032325](https://github.com/djshellshoxxx/humtrace/actions/runs/40109032325) completed successfully on all six build/test jobs; the release job was skipped on the PR branch. It tested documentation at `7afcf39c1816b8138ac0c545a70af2d60a8635b3`, not the report-verifier code.
- Source CI [40109648042](https://github.com/djshellshoxxx/humtrace/actions/runs/40109648042) passed all six native build/test jobs on `bec24d7086a212b4b4b82640a5f53d480d34f90f`; the release job was skipped on the PR branch.
- Draft PR #9 is open: https://github.com/djshellshoxxx/humtrace/pull/9. Update its body after pushing this handoff.
- Preserve published preview v0.2.5: tag object `6ca598bcdda17384b4bb924cec2d1ec9f005c3c6`, peeled source `d9a24852024e86bff7e25d64bd50a73d02944c6c`. No successor release was published; the release workflow was skipped for the PR branch.

## Completed provenance work

The Windows package workflow stages exact-version MSYS2 source-only recipe
archives for bundled runtime DLL owners. It validates package versions,
official primary/Tier 1 mirrors and source paths; caps downloads at 256 MiB per
archive and 512 MiB aggregate; and records local SHA-256 values. CI run
40108498575 staged 21 recipe archives for 22 owners. The recipes and patches
do not contain the referenced upstream sources.

The macOS workflow maps each packaged Mach-O image by exact filename against
installed Homebrew Cellar kegs. It records unique, ambiguous, and unmapped
cases, formula license/source metadata, hashes of matching keg files with
512 MiB per-file / 2 GiB aggregate limits, and installed formula recipes when
available. Seven fixture tests pass. Run 40108498575 confirmed 25 unique
formula matches and no ambiguous matches on ARM64; Intel reported 23 unique
matches and no ambiguous matches. The two unmapped images on each platform
were HumTrace's own CLI and GUI. Both macOS archives passed extracted smoke
tests. This is a candidate provenance aid, not binary identity, complete
source, license applicability, or legal proof.

HumTrace is labeled a software artwork in the README. Its first plugin workflow
is specified as a transparent live DAW capture effect followed by offline
analysis. VST3 and CLAP are selected, with REAPER 7.83 as first host target and
mono/stereo F32, 8–192 kHz, 60-second default and 1–120-second bounds. No
capture worker, adapter, plugin binary or tested host support is implemented.
The current executor has no REAPER binary; manual host validation remains
pending.

## Test and artifact evidence

All 62 local portable packaging/provenance tests passed:

- `python3 tests/test_packaging.py` — 2
- `python3 tests/test_package_licenses.py` — 12
- `python3 tests/test_msys2_sources.py` — 12
- `python3 tests/test_qt_sources.py` — 5
- `python3 tests/test_macos_packaging.py` — 14
- `python3 tests/test_macos_provenance.py` — 7
- `python3 tests/test_package_source.py` — 6
- `python3 tests/test_source_metadata.py --cmake /workspace/humtrace-tools/cmake/data/bin/cmake` — 4

`HUMTRACE_REQUIRE_JSONSCHEMA=1 make test` passed locally after the report
verifier change, including all C++ unit tests, CLI/streaming/paging/detector
integration tests and portable packaging tests. The new verifier suite also
passed independently. Native source CI 40109648042 then passed Linux core/CLI,
sanitizers, Linux GUI, Windows, macOS 15 Intel and macOS 26 ARM64; its release
job was skipped on this PR branch. It generates a small synthetic WAV and
report, validates the published schema, checks exact source byte length/hash
and detached report hash sidecars, and rejects duplicate keys, `NaN`/overflow
numbers, invalid UTF-8, deeply nested JSON, unsupported schema versions,
invalid schema values, reports over 64 MiB, changed source bytes, sidecar
mismatches and digest-file overwrite attempts. Python compilation,
`git diff --check` and staged-patch secret-pattern checks passed.

The source-checkout utility `tools/verify_report.py` requires Python
`jsonschema`; it accepts report JSON up to 64 MiB and streams source-file
hashing in 1 MiB chunks. It validates and compares bytes only. Desktop report
import, complete target metadata fields, provenance authenticity, and custody
remain unimplemented.

Run 40108498575 passed all six jobs: Linux core/CLI, sanitizers, Linux GUI,
Windows, macOS 15 Intel and macOS 26 ARM64. The separate release job was
skipped because this is a PR branch. All extracted package smoke tests passed.
Runner artifact archives and nested package checksums:

| Platform | Nested package SHA-256 | Outer Actions artifact SHA-256 |
| --- | --- | --- |
| Linux .deb | `364a0ed450b0c53bae0eeb110798a3144be573e14fc6d13c41674dcb7053dc77` | `5899f6c9dc762e7bec0929e53bed4c3d837166d56792768c4061836dcc59c03d` |
| Linux .tar.gz | `e70c3b3def80d77d919c5b546c916e9e86f2bac418888a460b14b68653bdf563` | same Linux artifact archive |
| Windows ZIP | `c75592454b947cf16b9763e23a67e44d2270f9317475069778aff0b1204f6b35` | `e8c0bcd903c7d7921ef80bf1d1a79e53cdcd53b9be541d324008732a6b21b630` |
| macOS ARM64 ZIP | `af3332101e8f4d710a06943d7f4644cf39a81f103187930aba5a5f6273f31c74` | `0d6a4dd31048d67d88e849e052d563eb7fca3f9672d7a180928ac443755973fb` |
| macOS Intel ZIP | `2e9cd3fd346356e1e57648f45d5a170d0b0110168eeccf2d801c09244e246f08` | `472d14ac0d17f6fcd9f43d3e1b7698e32e7dec68678fc137ace6d2f03ed604b0` |

Outer artifact archive hashes are not nested package hashes. A local byte-level
artifact audit remains pending because the GitHub artifact connector returns
an opaque file reference with no workspace-readable path. Do not bypass the
executor's HTTP 403 CONNECT proxy to download source archives; native runners
successfully collected the permitted MSYS2 recipes.

## Remaining work and gates

1. Push this handoff and acceptance-ledger update with a normal fast-forward,
   verify the exact remote ref, and refresh PR #9. The documentation push will
   start a separate CI run; track it without attributing its result to source
   run 40109648042.
2. Continue desktop report import and captured-audio provenance where feasible.
   Only source-tree report validation and digest checking are implemented;
   report import into the desktop, capture worker, and VST3/CLAP adapters remain
   unimplemented.
3. Complete artifact-specific licensing and corresponding-source review;
   recipe archives and Homebrew formula metadata are supplemental only.
4. Obtain held-out labeled recordings and calibrated uncertainty; verify
   accessibility, installation/upgrade and clean-machine workflows with
   humans on target platforms.
5. Validate VST3 and CLAP in the selected REAPER host on each supported
   platform before claiming plugin support. No new signing identity is
   available; Windows remains unsigned and macOS ad-hoc signed/not notarized.

Do not overwrite v0.2.5 or publish standalone packages as plugins. Any later
release must use a new version, accurate standalone/plugin scope, tested native
assets, source SHA, checksum manifest and installation guidance. A preview may
remain prerelease while external acceptance gates are open.
