# Development status — 2026-10-24

Active integration branch: `codex/humtrace-release-0.2.4`, at the verified
v0.2.5 preview source `d9a24852024e86bff7e25d64bd50a73d02944c6c`. Main remains
an older baseline. Existing branches, immutable tags and releases are preserved.

This continuation adds explicit detector settings and honest resolution/uncertainty
metadata, conservative candidate split/merge events across retained and disk-backed
storage, stronger provenance consistency checks, comparison settings snapshots,
locale/encoding/extreme-number fixes, desktop accessibility alternatives and release
runtime dependency checks. See the [acceptance ledger](release/acceptance-0.2.4.md),
[current QA evidence](testing/qa-0.2.4.md) and [release notes](release/release-notes-0.2.5.md).

The 0.2.5 patch supplies source-revision metadata explicitly and checks it in each
installed/extracted native package. Download verification found that the 0.2.4
Windows package's embedded revision was `unknown`; its release-level source SHA
and exact-tag CI trace were correct. That published tag and its assets are preserved.

The package workflow includes a checksum-verified upstream QtBase source baseline,
Windows runtime/plugin package and notice metadata, and a macOS per-image dependency,
minimum-OS and hash manifest. These are provenance inputs, not a compliance finding.
The Windows workflow now also stages the exact-version MSYS2 source-only recipes
for bundled runtime DLL owners. Its fixture tests pass; live collection was denied
by the saved executor's HTTP 403 network proxy, so the runner-side fetch and next
Windows archive remain unverified. Recipe archives are not upstream source archives.
Windows packages must pass an extracted-package smoke test with toolchain paths
removed. macOS packages must pass dependency closure, declared minimum-OS and ad-hoc
signature checks, followed by an extracted-package smoke test. Native baselines are
macOS 26 ARM64 and macOS 15 Intel. A published preview links the exact-tag CI result,
source SHA and asset checksums; only those results establish artifact availability.

The desktop and CLI are standalone applications. VST3/CLAP remain a scoped capture
interface proposal pending a product/host decision. Held-out labeled real-data
calibration, native screen-reader/high-DPI/clean-machine acceptance, signing identities,
full report import/verification and artifact-specific licensing review remain open.
These gates are not closed by synthetic test counts or the presence of a package.

The README labels HumTrace as a software artwork and audio analysis project. The
label describes the project form and does not change the open acceptance gates.
