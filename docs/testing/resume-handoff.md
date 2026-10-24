# HumTrace continuation handoff — 2026-10-24

## Current checkpoint

- Repository: `djshellshoxxx/humtrace` only.
- Branch: `codex/humtrace-release-0.2.4`.
- Verified implementation checkpoint and remote branch SHA: `889065fcd8e67fc1a3e3dc4983a6fb19a7348c54`.
- Pull request: [draft PR #9](https://github.com/djshellshoxxx/humtrace/pull/9), open and mergeable at the last check.
- Published preview: [v0.2.5](https://github.com/djshellshoxxx/humtrace/releases/tag/v0.2.5), tag object `6ca598bcdda17384b4bb924cec2d1ec9f005c3c6`, peeled source `d9a24852024e86bff7e25d64bd50a73d02944c6c`. Preserve that release and its assets.
- New source-recipe CI run: [40105892447](https://github.com/djshellshoxxx/humtrace/actions/runs/40105892447), head SHA above. It was in progress at this handoff; poll it before accepting the native Windows source download.

## Completed at this checkpoint

The Windows package workflow stages exact-version MSYS2 source-only recipe
archives for the package owners of bundled runtime DLLs. It validates package
page versions, source-link hosts and filenames, redirects, and bounded download
sizes. Shared archives are downloaded once. The manifest distinguishes locally
computed archive hashes from upstream-authenticated checksums and states that
these archives contain recipes and patch payloads, not their upstream sources.
The Windows ZIP includes the staged records. CI runs the focused fixture suite.

The README identifies HumTrace as a software artwork and audio-analysis
project. The release checklist and provenance specification now describe the
v0.2.5 evidence and open acceptance accurately. After checkpoint 889065f, the
working-tree plugin specifications select live DAW capture through both VST3
and CLAP, REAPER 7.83 as the initial manual host, and conservative mono/stereo,
F32, 8–192 kHz, 1–120-second bounds. This is a design decision only: there are
no capture/job or plugin adapter sources yet. The draft PR description records
the source-recipe follow-up and its live-download blocker.

Local verification passed all 54 packaging regression tests:

- `python3 tests/test_packaging.py` — 2
- `python3 tests/test_package_licenses.py` — 12
- `python3 tests/test_msys2_sources.py` — 11
- `python3 tests/test_qt_sources.py` — 5
- `python3 tests/test_macos_packaging.py` — 14
- `python3 tests/test_package_source.py` — 6
- `python3 tests/test_source_metadata.py --cmake /workspace/humtrace-tools/cmake/data/bin/cmake` — 4

`py_compile` and `git diff --check` passed. A staged-patch scan found no
credential or private-key patterns. The current C++ CTest result remains the
previous checkpoint's 29/29 and is not a test of these Python packaging changes.

## Open gate and next actions

The direct request to `https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qt6-base`
for installed version `6.12.0-1` failed at the executor's network proxy with
HTTP 403 during CONNECT. A matching `curl` header request had the same result.
No page or archive was downloaded and no source bundle was written locally.
The block is on live collection from this executor; the independent native
Windows CI runner is now attempting the same workflow. Do not bypass the proxy.

After CI finishes, record the exact run result. If it passes, retain the new
source checkpoint and continue with Mac dylib-to-Homebrew formula mapping and
other bounded provenance work. If it fails, preserve its job log and adjust
only from the actual failure evidence.

Report import/verification, capture provenance and plugin implementations are
still incomplete. VST3 and CLAP remain unimplemented; the standalone packages
are not plugin artifacts. Held-out recording validation, calibrated
uncertainty, platform accessibility/clean-machine acceptance, complete
artifact-specific licensing review, and signing/notarization remain open.
Windows is unsigned; macOS signatures are ad-hoc. Do not publish a successor
release until its tests, native artifacts, notes, checksums, and remaining gates
are reviewed. No new credentials, identities, purchases, or agreements are
available or authorized.
