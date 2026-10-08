# Detector validation corpus and evaluation specification

## Purpose and scope

Measure how well HumTrace estimates spectral components and groups events under stated conditions. The corpus does not turn frequency evidence into physical-source identification unless labels are independently established and the target inference is separately designed. Synthetic tests validate implementation and known-signal measurement; they do not establish field performance.

## Corpus layers

1. **Unit fixtures:** exact known samples and narrow parameter sweeps, generated deterministically with seed/version/parameters recorded.
2. **Controlled recordings:** known signal injected into varied recording paths, rooms, microphones/interfaces, sample rates, gains, cable/power configurations, and devices. Record ground truth independently and document uncertainty.
3. **Natural recordings:** consented or appropriately licensed representative recordings from diverse devices/environments, annotated by qualified reviewers with measured event boundaries and ambiguity labels.
4. **Negative/near-negative set:** musical tones, alarms, fans/transformers, speech/music, codec artifacts, isolated harmonics, 50/60-adjacent non-mains tones, switching tones, broadband noise, and transient clicks.

## Annotation format

Each clip has a stable opaque ID, content hash, source/consent/license record held separately, recording chain metadata, sample properties, annotation tool/version, annotator, event intervals, channel, measured fundamental/partials, uncertainty, source evidence class (`controlled`, `corroborated`, `unknown`), and disagreement state. Do not distribute private identities or sensitive location metadata. Keep audio and labels versioned; never amend labels silently.

## Data split and leakage control

Group by source recording session, physical device, location, and derived clip family before splitting. Development set is for threshold selection; validation set for tuning/ablation; held-out test is frozen until a release candidate. No overlapping slices, resampled duplicates, or same injected base waveform may cross split boundaries. Record split manifest digest.

## Metrics

- Peak frequency absolute error (Hz) and relative error by frequency band, SNR, duration, and sample rate.
- Amplitude error (dB) against known injected level; separately state peak vs RMS reference.
- Precision, recall, F1, false positives/hour, event onset/offset error, track fragmentation, and identity switches.
- Calibration error only if probabilistic output is later introduced; evaluate reliability curves and Brier/log loss on independent held-out data.
- Report confidence intervals/bootstrap method and subgroup counts. Empty/invalid categories are “not evaluated,” never zero.

## Release gate

Pre-register each detector threshold and primary metric before hold-out evaluation. Publish corpus composition, excluded cases, software commit, settings, and results. Set numerical pass thresholds after pilot data and user risk review; do not invent targets from synthetic fixtures. If a subgroup lacks enough examples, disclose it and use descriptive behavior only. Retest after any parser, DSP, threshold, or decoder update.

## Reuse and storage

Keep the corpus manifest schema independent from GUI and analysis C++ code. Store small synthetic WAVs in-repo; large controlled/consented recordings in a separately governed dataset with public fixture IDs, hashes, and access instructions. The test runner must be able to run unit fixtures offline and clearly skip unavailable external corpus segments.
