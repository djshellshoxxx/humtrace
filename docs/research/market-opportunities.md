# HumTrace market opportunities

**Reviewed:** 2026-10-08  
**Basis:** Qualitative product opportunities inferred from the project brief and the official feature/manual pages summarized in [competitor-analysis.md](competitor-analysis.md). This document makes no market-size, adoption, or revenue forecast. These are hypotheses to validate with interviews and prototype use, not claims of proven demand.

## Opportunity thesis

HumTrace can occupy a focused niche between **audio repair** and **general signal inspection**: help a user answer “what is in this recording, when does it occur, how strong is the evidence, does another recording contain a similar measured pattern, and how can I share the result?” Existing tools already provide important components—RX provides dedicated de-hum and restoration, Audacity offers spectrogram-based inspection and editing, Sonic Visualiser enables rich visualization/plugin analysis, REW provides measurement/RTA, and command-line/library ecosystems provide flexible DSP primitives. The opportunity is to integrate the investigation steps into one repeatable, comprehensible, file-first workflow.

The product should earn trust by making narrow claims. It can measure stable or drifting spectral energy and describe how observations resemble a mains-related harmonic pattern. Frequency resemblance alone cannot prove a ground loop, appliance, room, grid connection, recording date, or shared source.

## User groups and jobs to validate

| Candidate user group | Job to be done | Current workaround implied by available tools | HumTrace opportunity | Evidence needed before prioritizing |
|---|---|---|---|---|
| Home/studio musicians and recording engineers | Find a persistent hum, buzz, whistle, or intermittent interference in a track, then decide where to troubleshoot or repair | Inspect a spectrum/spectrogram manually, try a de-hum tool, or move among RX/Audacity and analyzer utilities | One-click file scan with channel-specific candidate events, harmonic evidence, A/B audition, and exportable diagnostics | Interviews with engineers; test whether event cards reduce time to diagnosis versus RX/Audacity workflows. |
| Live-sound, DJ, and field-recording technicians | Diagnose recordings captured from unfamiliar rigs or venues and compare takes or channels | Use spectrum/RTA, listen, inspect meters, then write notes or share screenshots | Batch reports that point to time ranges and channels, with before/after or recording-to-recording comparisons | Observe actual troubleshooting sessions; identify common file formats, acceptable report fields, and whether offline analysis fits turnaround constraints. |
| Audio/video post-production and podcast teams | Triage many recordings, locate recurring electrical noise, and hand precise evidence to an editor | Listen through files, view spectrograms, use denoise/de-hum modules, or script checks | Batch CLI plus GUI review; JSON/HTML output with exact event timestamps to guide repair in existing editors | Validate demand for batch speed, integration exports, and stable operation on long files. |
| Electronics and equipment troubleshooters | Determine whether recorded audio contains repeatable tonal, switching-like, or burst noise associated with a setup | General spectrum tools, test gear, and manual comparison; recordings may not be calibrated | Compare controlled captures, show drift and channel relationships, and suggest cautious next tests | Confirm that audio captures are a useful diagnostic input and identify which measured features lead to actionable tests. |
| Journalists, investigators, and forensic audio practitioners | Document what was measured in an audio recording and preserve an auditable analysis trail | Specialized tools, expert workflows, or custom scripts | Hash-anchored, reproducible reports that preserve settings and clearly separate measurement from interpretation | Specialist review, validation corpus, reproducibility review, and legal/forensic methodology assessment before claiming evidentiary suitability. |
| Developers, labs, and automated media pipelines | Run consistent diagnostics over folders and consume structured results | Python notebooks, FFmpeg/SoX commands, and custom SciPy/librosa code | Versioned CLI and JSON schema backed by a reusable C++ core | Prototype CLI adoption; test schema stability and dependency packaging. |

## Prioritized opportunities

Scores are relative planning judgments, not user-research results. **Cost** is estimated implementation burden; **benefit** is expected usefulness if the hypothesis is validated.

| Rank | Opportunity | Benefit | Cost | Why it is promising | Suggested stage |
|---:|---|---|---|---|---|
| 1 | Explainable interference scan with candidate timeline | High | Medium | Joins spectrum and spectrogram inspection to concrete timestamps, per-channel measurements, persistence, and local noise context. This is the clearest core product distinction. | Beta v0.1 core |
| 2 | Mains-harmonic candidate view | High | Medium | Users can inspect several partials and shared drift rather than relying on a single 50/60 Hz peak or undifferentiated spectrogram. Report the evidence and alternative interpretations. | Beta v0.1 core |
| 3 | Reproducible JSON/HTML diagnostic report | High for handoff users | Medium | Makes settings, hashes, software versions, result units, evidence, and caveats portable. This serves technicians, production handoff, and scripted review. | JSON in v0.1; HTML when charts stabilize |
| 4 | Channel-aware inspection and comparison | High | Medium | A hum may be present in one channel or differ in amplitude/phase. Showing independent channel analysis and synchronized comparisons is actionable and explainable. | v0.1 after basic detector |
| 5 | Batch folder scan and CLI | High for recurring workflows | Medium | Automates repetitive triage without requiring users to build pipelines from shell filters or notebooks; shares the same tested analysis core as the GUI. | v0.1 if the core and schema are stable; otherwise next release |
| 6 | Recording-to-recording fingerprint comparison | Medium-high | High | Could help find recurring measured patterns across takes, locations, or devices, but alignment, encoding, gain, and noise conditions complicate similarity. | Post-v0.1 after validation |
| 7 | Interference audition and candidate extraction | Medium-high | Medium-high | Lets users hear a suspected band or candidate component while preserving original audio as reference. Processing artifacts can mislead, so clear derived labeling and safe gain are essential. | Basic band-limited monitoring in v0.1; extraction later |
| 8 | Hardware troubleshooting assistant | Medium | Medium | Could translate patterns into practical checks, but attribution is underdetermined. Provide “possible next checks” tied to measured evidence; avoid definitive device/source labels. | Later, after user research and expert review |
| 9 | Educational signal explorer | Medium | Medium | Could teach how mains fundamentals, harmonics, drift, and broadband noise appear in a spectrum, improving approachability. | Post-v0.1; validate onboarding value |
| 10 | Versioned signature library | Medium | Medium-high | Generated examples and documented real examples could support education and tests. External recordings need rights/provenance; library similarity must not imply same source. | Build synthetic fixtures first; curated library later |
| 11 | ENF matching or forensic dating | Unclear / high scrutiny | Very high | Published research shows matching depends on recording and reference conditions. It carries substantial validation and interpretation burden and does not belong in a basic hum detector. | Research-only until method, reference coverage, and independent validation exist |

## Positioning and packaging

### Recommended positioning

> **HumTrace measures and documents electrical and electronic interference patterns in audio recordings.**

Lead with “find, measure, compare, and document.” Describe source categories as candidate interpretations supported by named measurements. Keep audio cleanup as a supporting workflow, not the headline promise.

### Packaging ladder to test

1. **Standalone desktop application:** Primary experience for engineers and technicians who need to inspect files visually and audition regions.
2. **CLI and JSON:** Low-friction entry for batch review, QA scripts, and users already working with FFmpeg/SoX/Python.
3. **Reusable analysis library:** Supports integration into future Circuit Drift Labs tools and external developer workflows. Keep GUI, JUCE wrapper, and report rendering outside the signal-analysis core.
4. **Optional plugin adapter:** Consider only after the offline product is working. A plugin may provide quick analysis while editing, but expensive fingerprinting/reporting is better suited to offline/background execution and must never block an audio callback.

## Product gaps worth validating first

- **Workflow friction:** Can users move from “something sounds wrong” to a useful event and precise timestamp more quickly than with a general spectrogram or repair suite?
- **Measurement trust:** Do users understand frequency, dBFS, local noise, persistence, and evidence components without specialist training?
- **False positives:** How often do music notes, harmonics of instruments, resonances, codecs, or transient events resemble electrical interference?
- **Comparison meaning:** Which invariances matter to users—frequency profile, relative harmonic levels, drift trajectory, event cadence—and what recording differences make comparisons unreliable?
- **Report utility:** Who receives reports, which plots/data they need, and whether JSON or HTML is more useful in real workflows?
- **Formats and scale:** Which source formats dominate and how long are typical recordings? These determine decoder choice, streaming requirements, and batch throughput.
- **Willingness to adopt:** Does a free/open source diagnostic tool gain use through the CLI/library, while a polished GUI/report supports donations, sponsorship, or future paid distribution? No pricing or monetization recommendation is justified without user research.

## Validation plan

1. Conduct short interviews across studio, live-sound, post-production, electronics troubleshooting, and forensic user groups. Ask for a recent real interference-diagnosis example and observe the actual workflow rather than asking only about hypothetical interest.
2. Build a clickable or instrumented prototype around one job: load a recording, review per-channel candidates, inspect a harmonic group and timeline, then export a report.
3. Compare task time, missed events, false alarms, and interpretation accuracy against participants’ current mix of RX, Audacity, Sonic Visualiser, REW, and scripts.
4. Evaluate detectors against synthetic ground truth plus rights-cleared, labeled real-world material. Keep tuning and held-out data separate. Publish conditions and error rates before presenting numerical accuracy claims.
5. Test usefulness of reports with a handoff recipient, not only the person who ran the analysis. Record whether timestamps, plots, settings, hashes, and caveats are sufficient to reproduce or act on a finding.
6. Revisit prioritization after this evidence. Delay expensive signature matching, source attribution, ENF matching, and deep repair features until a validated need and achievable reliability are demonstrated.

## Source basis

Official feature pages/manuals were accessed 2026-10-08. The opportunity statements are product hypotheses inferred from this capability review and the HumTrace brief.

- iZotope RX features: https://www.izotope.com/en/products/rx/features
- Audacity noise reduction: https://www.audacityteam.org/features/noise-reduction/
- Audacity spectrogram manual: https://manual.audacityteam.org/man/spectrogram_view.html
- Sonic Visualiser features: https://www.sonicvisualiser.org/features.html
- REW overview: https://www.roomeqwizard.com/help/help/html/welcome.html
- Spek about/features: https://www.spek.cc/about
- FFmpeg filters: https://ffmpeg.org/ffmpeg-filters.html
- SoX command manual (Debian testing manpage mirror): https://manpages.debian.org/testing/sox/sox.1.en.html
- librosa STFT API: https://librosa.org/doc/main/api/generated/librosa.stft.html
- SciPy signal API: https://docs.scipy.org/doc/scipy/reference/signal.html
- HumTrace product brief and existing repository scope: `docs/specs/product-requirements.md`, `docs/research/dsp-algorithms.md`, and the attached master engineering prompt.
