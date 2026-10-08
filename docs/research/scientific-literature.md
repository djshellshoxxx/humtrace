# Scientific and engineering literature for HumTrace

**Research snapshot:** 2026-10-08  
This is a focused bibliography for the current product scope, with a short note on what each source supports and what it does not establish. Access date for all links: **2026-10-08**.

## Forensic ENF

### Grigoras (2007), “Applications of ENF criterion in forensic audio, video, computer and telecommunication analysis”

[PubMed record](https://pubmed.ncbi.nlm.nih.gov/16884872/) · [ScienceDirect record](https://www.sciencedirect.com/science/article/pii/S0379073806004312)

Describes extracting the 50/60 Hz network-frequency-related signal and comparing it with reference data for forensic questions. Useful for defining ENF as a sequence/reference-comparison method rather than a simple hum detector. The paper’s stated applications do not make an uncorroborated spectral peak conclusive evidence of time, location, or authenticity.

### Grigoras et al. (2005), “Application of the Electrical Network Frequency (ENF) Criterion: A case of a digital recording”

[PubMed record](https://pubmed.ncbi.nlm.nih.gov/16226153/) · [ScienceDirect record](https://www.sciencedirect.com/science/article/pii/S0379073804007662)

Reports a forensic validation program and a case application based on extracting an ENF sequence and comparing with reference data. Supports the need for reference data and case-specific validation; it is not evidence that all recordings contain a usable ENF signal.

### Huijbregtse & Geradts (2009), “Using the ENF Criterion for Determining the Time of Recording of Short Digital Audio Recordings”

[Author-upload record](https://www.researchgate.net/publication/221577615_Using_the_ENF_Criterion_for_Determining_the_Time_of_Recording_of_Short_Digital_Audio_Recordings)

Specifically studies short recordings and ENF time matching. Relevant to ambiguity and limited sequence information in short clips. Treat any minimum-duration rule as method- and data-dependent, not a universal cutoff.

### ENFSI Forensic Speech and Audio Analysis Working Group, ENF best-practice guideline

[Guideline PDF](https://enfsi.eu/wp-content/uploads/2016/09/forensic_speech_and_audio_analysis_wg_-_best_practice_guidelines_for_enf_analysis_in_forensic_authentication_of_digital_evidence_0.pdf)

Practice guidance calls for documenting analysis and limitations, with method selected for the case and the available evidence (including recording length). It informs HumTrace’s reporting recommendations; the software does not claim ENFSI compliance or validated casework performance.

### IEEE (2023), “ENF Based Digital Multimedia Forensics: Survey, Application, Challenges and Future Work”

[IEEE Xplore abstract](https://ieeexplore.ieee.org/abstract/document/10239381)

Survey of ENF extraction and proposed forensic applications, including challenges. Useful overview; application areas discussed in a survey should not be interpreted as generic validation of source attribution or every individual implementation.

### Hua et al. (2021), “Robust ENF Estimation Based on Harmonic Enhancement and Maximum Weight Clique”

[IEEE DOI](https://doi.org/10.1109/TIFS.2021.3099697) · [arXiv preprint](https://arxiv.org/abs/2011.03414) · [authors' ENF-WHU repository](https://github.com/ghua-ac/ENF-WHU-Dataset)

Studies multi-harmonic ENF enhancement and harmonic selection, and reports evaluation with synthetic signals and the ENF-WHU dataset. This is a relevant future method candidate if HumTrace implements ENF extraction. It does not justify applying the algorithm to unrelated HumTrace detection or treating an extracted sequence as uniquely identifying a date/location. Inspect the paper's evaluation conditions, repository revision, dataset license/consent, and reference-data assumptions before reusing code or data.

### ENF-WHU audio dataset and companion code

[Upstream GitHub project](https://github.com/ghua-ac/ENF-WHU-Dataset)

The project describes a real-recording dataset and MATLAB programs for ENF detection, enhancement and estimation. This can inform an independent ENF-method evaluation, subject to verification of exact dataset files, annotations, split protocol, license, and recording conditions. It is not a general audio-interference corpus; the target task and population are specifically ENF-related.

## Spectral estimation and sampling

### Welch (1967), “The Use of the Fast Fourier Transform for the Estimation of Power Spectra: A Method Based on Time Averaging Over Short, Modified Periodograms”

[IEEE DOI](https://doi.org/10.1109/TAU.1967.1161901) · [SciPy implementation reference](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html)

Foundational averaged-periodogram PSD method. Welch averaging reduces estimator variance relative to a single periodogram at the cost of segment-length-limited resolution. SciPy documents practical window, overlap, scaling and averaging controls. For HumTrace, PSD should be used for stable level/noise characterization; framed spectra are needed for time tracking.

### Bartlett (1950), “Periodogram Analysis and Continuous Spectra”

[Biometrika DOI](https://doi.org/10.1093/biomet/37.1-2.1) · [SciPy Welch documentation and references](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html)

Foundational periodogram averaging reference. Relevant to the resolution-versus-variance tradeoff underlying Welch-style estimation.

### SciPy Signal reference: periodogram and Welch

[Periodogram](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.periodogram.html) · [Welch](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html)

Authoritative implementation documentation for spectrum/PSD scaling, one-sided output, windowing, segment size, overlap and averaging. This is engineering documentation, not a substitute for validation against known signals and independent implementations.

### Analog Devices, “Basics of Band-Limited Sampling and Aliasing”

[Technical article](https://www.analog.com/en/resources/technical-articles/basics-of-bandlimited-sampling-and-aliasing.html)

Explains spectral replication, Nyquist conditions, aliasing, and why filtering before sampling matters. Supports a key limitation: the apparent in-band frequency in a digital recording may be an alias of out-of-band content.

## Coupling mechanisms and audio engineering

### Whitlock & Fox, “Ground Loops: The Rest of the Story” (AES Convention paper)

[AES-hosted PDF](https://www.jensen-transformers.com/wp-content/uploads/2015/02/AES-Ground-Loops-Rest-of-Story-Whitlock-Fox-Generic-Version.pdf) · [AES e-Library entry](https://secure.aes.org/forum/pubs/conventions/?elib=15656)

Audio-specific engineering analysis of magnetic induction in premises wiring, ground-current coupling and hum/buzz mechanisms. Supports multiple candidate routes for low-frequency interference and the need not to use “ground loop” as a catch-all label. It does not provide a method to identify the route from an audio spectrum alone.

### AES27-1996 (s2012), “Managing Recorded Audio Materials Intended for Examination”

[AES standards page](https://aes.org/publications/standards-store/?id=29)

Forensic-audio material-management practice. Relevant to preserving originals and documenting derived material. The standard is paywalled; this citation does not claim HumTrace is compliant.

### Bel Fuse, “EMI Considerations for Switching Power Supplies”

[Application note](https://www.belfuse.com/resource-library/tech-paper/emi-considerations-for-switching-power-supplies)

Discusses switching supply conducted/radiated emissions and mitigation. It supports the plausibility of switching electronics as interference sources, but not a specific spectral-to-device classification rule.

### Texas Instruments, “USB Audio Isolation with Isolated USB 2.0 Redriver”

[Application note PDF](https://www.ti.com/lit/pdf/slla601) · [SLLA580 application brief](https://www.ti.com/document-viewer/lit/html/SLLA580)

Engineering documentation on USB audio isolation and ground-potential/noise concerns. Supports USB as a possible coupling context, not proof of USB causation from an audio recording.

## Source interpretation and product implications

The literature supports reproducible measurements of spectral peaks, PSD, temporal tracks, harmonic relationships and channel relationships. It supports cautious discussion of candidate mechanisms. It does not provide a universal, validated classifier that maps an arbitrary audio-only spectrum to “ground loop,” “USB,” a named piece of equipment, location, device identity, or recording authenticity.

Accordingly, HumTrace’s evidence hierarchy should be: (1) measured quantities and method settings; (2) pattern description; (3) explicitly qualified candidate explanations; (4) external corroboration clearly labeled as such. ENF reference matching, source fingerprinting and probability-of-cause outputs remain research hypotheses until validated on representative, labeled recordings with known capture chains and error rates.
