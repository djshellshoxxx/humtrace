# Deferred feature specification: ENF extraction and reference comparison

**Release status:** excluded from beta v0.1. This is a research-stage scope so any later implementation starts from validated requirements. ENF matching is not a fixed 50/60 Hz detector.

## Purpose and proposition

Estimate a time-varying mains-frequency-related signal from a recording and compare the extracted sequence to a user-supplied, documented reference sequence. At most, a validated method can report consistency with a reference under stated conditions. It cannot independently prove recording date, location, authenticity, continuity, or absence of editing.

## Prerequisites

- The v0.1 spectral estimator, track model, uncertainty reporting, input hashing, and validation corpus are stable.
- Reference data has provenance, coverage interval, sampling/measurement method, network region metadata, licensing/permission to use, integrity digest, and uncertainty. No scraping or implicit network upload.
- A published extraction/matching method is selected and independently implemented or validated. Research must characterize minimum useful duration and signal quality from data, not hardcode a universal cutoff.
- Evaluate methods such as Hua et al.'s multi-harmonic enhancement/selection only against the stated task and test design. ENF-WHU is a candidate research corpus, not a substitute for independent data and not necessarily licensed for redistribution.
- Legal and privacy review covers reference data and any location/network metadata.

## Processing pipeline

1. Select a documented band around nominal 50/60 Hz; retain original samples and channels.
2. Estimate the mains-related component per channel using a method selected by preregistered benchmark (narrowband filtering, phase tracking, or frequency-domain estimation). Track amplitude, SNR, continuity, frequency, and uncertainty per time interval.
3. Reject/flag clipped, filtered, resampled, unstable-clock, weak, absent, or ambiguous segments. Do not bridge missing data invisibly.
4. Compare the usable sequence to reference records across an explicitly specified time-offset range. Preserve all candidate offsets and score surfaces; report uniqueness/ambiguity, coverage gaps, and sensitivity to method parameters.
5. Show reference identity/version and exact comparison settings. Never silently query a server or upload user audio.

## Output and claims

Output includes extracted sequence, quality diagnostics, reference digest and provenance, comparison method/version, candidate offsets/scores, uncertainty and alternative matches, preprocessing, and warnings. Labels are “consistent with this reference over this interval” only after target-specific validation. Avoid “recorded at,” “authentic,” or “not edited.” A mismatch is not proof of manipulation; absence of an extractable signal is “inconclusive.”

## Validation gate

Use controlled recordings with known clock/date and independently acquired reference series, multiple grids/networks, recording chains, durations, SNRs, compression/resampling, edits, and deliberately ambiguous/repeated sections. Split by recording session and reference interval. Report false-match, missed-match, time-offset error, and inconclusive rate with confidence intervals. A forensic claim requires practitioner/lab review beyond software unit tests.

## Sources

- [Scientific literature: ENF section](../research/scientific-literature.md), including Grigoras (2007), Grigoras et al. (2005), Huijbregtse & Geradts (2009), and ENFSI best-practice guidance.
- Hua et al. (2021), [robust multi-harmonic ENF estimation](https://doi.org/10.1109/TIFS.2021.3099697), and the [ENF-WHU dataset/code repository](https://github.com/ghua-ac/ENF-WHU-Dataset); review dataset terms and methodology before use.
- [Forensic limitations](../research/forensic-limitations.md), which details clock drift, short clips, absent/filtered ENF, reference coverage, ambiguity, and inference limits.
- This specification does not state that HumTrace is ENFSI-compliant or validated for forensic casework.
