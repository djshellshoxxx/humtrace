# HumTrace 0.2.4 acceptance ledger

This is a development preview. A passing synthetic test is implementation evidence,
not a field detector accuracy or forensic authentication claim.

| Requirement | Implementation and evidence | Remaining acceptance |
|---|---|---|
| Detector configuration | Explicit frame/hop, spectrum/Welch mean removal, peak band/separation, local baseline neighborhood, thresholds and bounded nominal-harmonic search; effective settings in schema 1.3; CLI/desktop regressions | Only Hann is supported. Measured-fundamental family grouping and threshold validation on independent recordings remain open. |
| Uncertainty | Raw/refined frequency, bin spacing, Hann width, half-power width, floor/prominence, boundary/competition flags and method descriptions; absent estimates and calibrated uncertainty are null | No calibrated uncertainty or source-attribution probability model; held-out labeled recordings are unavailable. |
| Tracking | Conservative deterministic candidate graph split/merge/competing events with fresh segment IDs, explicit gaps, quartiles, drift fit quality and observed intervals; retained/paged/streamed equality including suppression | Graph events are possible associations, not physical lineage. Real-data identity switches, fragmentation and tolerance selection remain unvalidated. |
| Integrity | Input byte hash/length, bounded post-analysis consistency rehash, mutation and timestamp-restoration regression, locale-independent JSON/CSV, UTF-8 paths | No immutable input snapshot, report importer, report signature/digest or chain-of-custody workflow. |
| Comparison and labels | Explicit channel mapping, independent summaries with settings snapshots, deterministic assignment, finite-delta checks, note labels with extreme-input tests | No time alignment, resampling, source identity inference or validated similarity classifier. |
| Long files/export | Desktop and ordinary long/dense CLI use private disk-backed storage; explicit streaming quotas, bounded caches, incremental atomic JSON, cancellation/cleanup; one/three-hour acceptances | Combined CLI CSV uses the capped retained API. Default private-spool quota is 4 GiB; comparison summary cap is 16 MiB. Human/device acceptance remains separate. |
| Desktop access | Virtual waveform envelope and measurement tables/text alternatives, explicit units, fractional thresholds, no stale peak/digest text, real dialog/worker cancellation regressions | Packaged Narrator/AT-SPI/VoiceOver, high-DPI, focus visibility and clean-machine install/upgrade/uninstall require platform/human evidence. |
| Windows/macOS/Linux | Native CI builds/tests and extracted/installed package smoke; Windows recursive DLL closure, dependency/license inventory, source SHA/checksums; macOS separate ARM/Intel archives | Exact native run URLs/results establish artifact availability. Windows unsigned; macOS ad-hoc signed, not notarized. No new signing identity is available. |
| Licensing | Runner package license inventory and verified upstream QtBase source/license baseline accompany redistributed Qt packages; source and package manifests identify scope | Distribution patches, other dependencies, relinkability and product license choice need artifact-specific review; inventory alone is not legal approval. |
| VST3/CLAP | [Host/capture interface proposal](../specs/plugin-capture-interface.md) and [format assessment](../specs/plugin-adapter.md) | Decide capture workflow, target hosts and bounded capture limits. SDK integration, host validation and plugin artifacts are unimplemented. |

Fresh commands and actual measurements are recorded in [QA evidence](../testing/qa-0.2.4.md).
The release links its exact source SHA and checksums. Existing tags/releases are preserved.
