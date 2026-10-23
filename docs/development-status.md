# Development status — 2026-10-23

Active integration branch: `codex/humtrace-release-0.2.4`, descended from
`codex/humtrace-tracking-completion` and published preview v0.2.3. Existing branch,
tag and release histories are preserved; main is still an older baseline.

This continuation adds explicit detector settings and honest resolution/uncertainty
metadata, conservative candidate split/merge events across retained and disk-backed
storage, stronger provenance consistency checks, comparison settings snapshots,
locale/encoding/extreme-number fixes, desktop accessibility alternatives and release
runtime dependency checks. See the [acceptance ledger](release/acceptance-0.2.4.md),
[current QA evidence](testing/qa-0.2.4.md) and [release notes](release/release-notes-0.2.4.md).

The desktop and CLI are standalone applications. VST3/CLAP remain a scoped capture
interface proposal pending a product/host decision. Held-out labeled real-data
calibration, native screen-reader/high-DPI/clean-machine acceptance, signing identities,
full report import/verification and artifact-specific licensing review remain open.
These gates are not closed by synthetic test counts or the presence of a package.
