# HumTrace design and build index

This page is the entry point for product research, specifications, implementation order, and release checks. A design document records intended behavior; it does not mean the feature is implemented or validated. Current implementation status is tracked in [product requirements](../specs/product-requirements.md) and the [beta checklist](../release/beta-checklist.md).

## Read first

1. [Product requirements and delivery plan](../specs/product-requirements.md) — supported user workflows, acceptance goals, and release stages.
2. [Architecture overview](../architecture/overview.md) and [decisions](../architecture/decisions.md) — module boundaries and accepted/open decisions.
3. [Research decisions](../research/research-decisions.md) — dependency and algorithm recommendations with licensing constraints.
4. [GUI toolkit decision research](../research/gui-toolkit-evaluation.md) and [decision report](../research/gui-toolkit-decision-research.md) — current framework comparison and recommendation.
5. [Build order](../integration/build-order.md) — dependency-aware implementation sequence.

## Feature specifications

| Feature | Specification | Primary implementation boundary |
|---|---|---|
| Desktop shell, visual system, interactions | [GUI specification](../specs/gui.md) | `app/`; toolkit widgets and custom plots; no DSP in UI |
| PSD, peaks, harmonics, measured evidence | [Analysis engine](../specs/analysis-engine.md) | `dsp/`; deterministic per-channel measurements |
| Persistent tracks and event intervals | [Temporal tracking](../specs/temporal-tracking.md) | `analysis/`; stable identity and explicit gaps |
| Long recordings, cancellation, progress | [Long-file workflow](../specs/long-file-workflow.md) | `audio_io/` + worker/job controller |
| A/B comparison | [Recording comparison](../specs/recording-comparison.md) | `analysis/compare/`; never silently resample or align |
| Input hashes and reproducible reports | [Report integrity](../specs/report-integrity.md) | `report/`; evidence handling claims remain bounded |
| Decoder and format expansion | [Audio input support](../specs/audio-input-support.md) | `audio_io/`; preserve original samples and metadata |
| Accuracy and regression evidence | [Validation corpus](../specs/validation-corpus.md) | versioned fixtures, labels, metrics, and held-out tests |
| Installers and release verification | [Packaging and release](../specs/packaging-release.md) | native Linux/Windows jobs and reproducible artifacts |

Deferred from beta v0.1 but scoped for later design:

- [ENF extraction/reference comparison](../specs/enf-analysis.md)
- [Qualified troubleshooting guidance](../specs/diagnostic-guidance.md)
- [Reference signature library](../specs/signature-library.md)
- [Plugin adapters](../specs/plugin-adapter.md)

## Research notes

- [DSP methods](../research/dsp-algorithms.md)
- [Algorithm implementation equations and invariants](../research/dsp-algorithms.md#implementation-equations-and-invariants) — FFT amplitude/PSD scaling, peak interpolation, harmonic evidence, and track assignment conventions.
- [Detector method selection](../research/detector-method-selection.md)
- [Interference taxonomy](../research/interference-taxonomy.md)
- [Forensic limitations](../research/forensic-limitations.md)
- [Evidence handling and report boundaries](../research/evidence-handling.md)
- [Scientific literature](../research/scientific-literature.md)
- [Competitor analysis](../research/competitor-analysis.md)
- [Existing open-source components](../research/existing-open-source.md)
- [Market opportunities](../research/market-opportunities.md)
- [Sources and references](../research/sources-and-references.md)
- [Research method and archival provenance](../research/methodology-and-archival-provenance.md)
- [Detector method selection](../research/detector-method-selection.md)
- [Evidence handling](../research/evidence-handling.md)
- [GitHub project landscape](../research/github-project-landscape.md)
- [Textbooks, journals, and archives](../research/textbooks-journals-archives.md)

## Algorithm and validation handoff

Implementation work should use the algorithm equations and invariants in [DSP methods](../research/dsp-algorithms.md#implementation-equations-and-invariants) together with the public contracts in [analysis engine](../specs/analysis-engine.md) and [temporal tracking](../specs/temporal-tracking.md). Validate numerical values against analytical fixtures and a separately held-out corpus before treating any score as calibrated. The [validation corpus](../specs/validation-corpus.md) specifies leakage controls and metrics; the method-selection note specifies the experiment matrix. Preserve estimator settings and versions in each result so comparisons remain reproducible.

## Cross-cutting rules

- Analyze source channels independently; any derived downmix, alignment, or resampling must be an explicit operation recorded in settings.
- Keep measured values, algorithmic evidence, interpretation, and physical-source hypotheses separate in both APIs and UI.
- No confidence probability or device/ground-loop label ships without a representative, labeled, independently held-out validation set.
- Preserve the original file. Never modify it during analysis or comparison.
- All long-running decode, hash, DSP, and report work runs off the UI thread, supports cancellation, and has a documented memory bound.
- Every module spec must define public inputs/outputs, units, failure behavior, tests, dependency/license obligations, and a migration handoff.
