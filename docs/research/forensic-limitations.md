# Forensic limitations and interpretation safeguards

**Research snapshot:** 2026-10-08  
**Scope:** Limits on inferences from HumTrace measurements. This document is not a casework protocol or a claim that HumTrace is validated forensic software.

## Observation is not attribution

Audio analysis measures the final recorded signal after the acoustic environment, transducer, analog electronics, sampling, file encoding, and any later processing. A spectral peak can be described precisely while its cause remains underdetermined. A 50/60 Hz component or harmonic family does not by itself prove a mains connection, ground loop, specific appliance, recording device, location, or tampering.

Multiple physical paths can produce similar results: direct electrical coupling, common-impedance current, magnetic or capacitive pickup, RF demodulation, an acoustic source, a periodic mechanical source, nonlinear distortion, or editing/processing. Conversely, one source can look different after filtering, gain changes, channel mixing, compression, resampling or lossy encoding.

## Electrical Network Frequency (ENF)

ENF analysis is a specialized comparison method. It attempts to extract the time-varying mains-frequency-related component from a recording and compare its trajectory with reliable reference data from the relevant power network and period. It is not simply “detect a 50/60 Hz tone.” A fixed-frequency peak without a sufficiently resolved, stable time sequence and appropriate reference record is not an ENF match.

Published ENF literature reports applications to continuity/timestamp questions, but results depend on signal availability, extraction, recording duration, processing history, reference-data quality and uniqueness of candidate matches. Short clips may contain insufficient distinctive sequence information; repeated or similar grid-frequency patterns, fixed offsets and ambiguous alignments can impair uniqueness. A match should be expressed as support for consistency with a reference sequence under stated assumptions, not proof of when/where/how a recording was made. A mismatch may result from weak/absent ENF, clock or speed changes, resampling, filtering, editing, wrong reference coverage, or extraction error, and is not automatically proof of manipulation.

For HumTrace beta, ENF matching is explicitly outside the product claim. If an exploratory ENF display is later added, it should show extracted track and quality diagnostics only, require user-supplied reference data, retain alternate candidate matches, disclose reference coverage/resolution, and avoid date, location, authentication or tampering conclusions. Any casework use would require method validation, uncertainty characterization, examiner competence, and applicable laboratory procedures.

## Measurement limits

### Frequency and amplitude

- FFT bin spacing is determined by analyzed frame duration (Δf = sample rate / frame length). Zero-padding interpolates the displayed grid but does not add information or true resolving power.
- Window choice trades leakage behavior against main-lobe width and amplitude accuracy. Closely spaced components may merge; a non-bin-centered tone spreads over bins.
- Frequency estimates depend on sample-clock accuracy and any playback-speed change. Files carrying nominal sample-rate metadata do not prove the capture clock was exact.
- dBFS is a digital full-scale reference. Without microphone, analog-chain and calibration data, it is not SPL, voltage, field strength or equipment-emission level.
- A local noise estimate depends on the selected neighborhood and can be biased by nearby tones, modulation, nonstationary content, speech/music, transients or sparse spectra. Detection thresholds require validation on representative material.

### Time and stationarity

- STFT frame length and hop size trade frequency and time resolution. A long frame improves separation of nearby low-frequency tones but blurs event timing; a short frame does the reverse.
- A transient leaks across frequency bins within its frame; overlap can make one event appear in adjacent frames. Persistence/event duration must account for analysis framing and association logic.
- Averaged PSDs summarize energy and can hide intermittency; median or robust averages may suppress outliers but alter interpretation.
- Drift trackers may jump between neighboring peaks, harmonics, or unrelated sources. Track continuity is an algorithmic estimate and requires confidence/ambiguity reporting.

### Channel and phase

Inter-channel amplitude, phase or coherence can establish that components are related in the recorded channels under specified processing. It cannot alone identify the physical coupling route. Mono conversion, polarity changes, stereo widening, lossy coding or channel decorrelation can change or cancel evidence.

### Sampling and file processing

Analog content above Nyquist can alias into the digital band if not adequately filtered before conversion. Later sample-rate conversion can introduce filtering and alter phase/noise. Lossy codecs can spread, quantize, suppress or reshape low-level components; denoising, equalization, dynamic processing and editing can likewise change signatures. HumTrace should identify known metadata and analysis steps but cannot generally reconstruct an undocumented signal chain from the final file.

### Similarity and fingerprints

Two recordings with similar harmonic peaks or spectral envelopes may share a common environmental/electrical condition, use similar equipment, or simply contain coincident tones. Similarity is not device identification, common-location evidence, common-source proof, or proof that two excerpts share an original recording. Any score needs a defined feature representation, invariance tests, a representative known-source dataset, false-match and false-nonmatch rates, and calibrated decision thresholds. Without those, report descriptive feature distance only, not a probability or identity.

## Evidence strength language

HumTrace should separate:

1. **Measured values:** frequency, amplitude, bandwidth, duration, channel, drift, event times, PSD/prominence, estimator settings and uncertainty.
2. **Pattern description:** “persistent narrowband line,” “candidate integer-spaced peaks,” “periodic transient train,” or “broadband rise.”
3. **Possible explanation:** carefully qualified list of plausible mechanisms, with a statement that the audio alone does not resolve them.
4. **External corroboration:** user-entered or independently measured evidence, explicitly distinguished from what the audio analysis itself found.

Avoid unexplained single confidence percentages. A “confidence” score should be tied to a validated target proposition and dataset. For v0.1, component evidence (prominence, persistence, harmonic consistency, track stability, SNR estimate) is more transparent than a calibrated-sounding source probability.

## Reproducibility and evidence handling

For a defensible technical report, preserve the original file; calculate a cryptographic hash; record file metadata and tool/version; preserve analysis settings and software build; document any conversion or preprocessing; keep source audio separate from derived audio; make reports reproducible; and state limits, alternative explanations and uncertainty. HumTrace reports should never overwrite source files. These are product recommendations informed by forensic practice, not a claim of compliance with a specific standard.

## Sources

Accessed 2026-10-08.

1. ENFSI Forensic Speech and Audio Analysis Working Group, [Best Practice Guidelines for ENF Analysis in Forensic Authentication of Digital Evidence](https://enfsi.eu/wp-content/uploads/2016/09/forensic_speech_and_audio_analysis_wg_-_best_practice_guidelines_for_enf_analysis_in_forensic_authentication_of_digital_evidence_0.pdf). Emphasizes case-specific scope, full documentation and explicit limitations.
2. Catalin Grigoras, [Applications of ENF criterion in forensic audio, video, computer and telecommunication analysis](https://pubmed.ncbi.nlm.nih.gov/16884872/), Forensic Science International (2007); abstract/indexing and article record. Describes reference comparison, extraction and practical scope.
3. K. A. H. Huijbregtse and Z. J. M. H. Geradts, [Using the ENF Criterion for Determining the Time of Recording of Short Digital Audio Recordings](https://www.researchgate.net/publication/221577615_Using_the_ENF_Criterion_for_Determining_the_Time_of_Recording_of_Short_Digital_Audio_Recordings), 2009. Discusses short-recording limitations. The linked repository is an author-upload page; consult the publication itself for formal citation and full context.
4. IEEE, [ENF Based Digital Multimedia Forensics: Survey, Application, Challenges and Future Work](https://ieeexplore.ieee.org/abstract/document/10239381), survey (2023). Summarizes ENF application claims and challenges.
5. AES, [AES27-1996 (s2012): Managing Recorded Audio Materials Intended for Examination](https://aes.org/publications/standards-store/?id=29), standards landing page. The standard is available through AES; this document cites its scope and does not assert HumTrace compliance.
6. SciPy, [Welch PSD reference](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html) and [periodogram reference](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.periodogram.html), estimator, window, scaling and averaging parameters.
7. Analog Devices, [Basics of Band-Limited Sampling and Aliasing](https://www.analog.com/en/resources/technical-articles/basics-of-bandlimited-sampling-and-aliasing.html), aliases and limits on recovering sampled signals.
8. Whitlock and Fox, [Ground Loops: The Rest of the Story](https://www.jensen-transformers.com/wp-content/uploads/2015/02/AES-Ground-Loops-Rest-of-Story-Whitlock-Fox-Generic-Version.pdf), AES paper on plausible audio grounding/interference mechanisms.
