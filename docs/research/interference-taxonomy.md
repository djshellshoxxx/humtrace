# Audio interference taxonomy for HumTrace

**Research snapshot:** 2026-10-08  
**Purpose:** A vocabulary for describing audio observations without treating a spectral pattern as proof of its physical cause.

## Interpretation rule

HumTrace analyzes the recorded waveform. It can measure frequencies, levels, bandwidth, persistence, modulation, channel distribution and timing. Unless there is independent evidence (for example, controlled equipment substitution or electrical-field/current measurement), the physical source remains a hypothesis. “60 Hz component” is an observation; “ground loop” is a possible explanation. Similar patterns can arise through different electrical, acoustic, mechanical, recording-chain or post-processing paths.

Amplitude in a decoded digital file is measured relative to full scale (dBFS), not volts, magnetic-field strength, sound-pressure level, or a calibrated electrical quantity. An audio recording alone generally cannot distinguish conductive coupling from inductive/capacitive pickup, radiated EMI later demodulated in the chain, or an acoustic source.

## Taxonomy

| Measurable pattern | What can be reported | Possible mechanisms to investigate (not a diagnosis) | Useful discriminators / limits |
|---|---|---|---|
| Narrow low-frequency component near nominal 50 or 60 Hz | Peak frequency and uncertainty, level, persistence, drift, channel, and local-noise prominence | Mains-frequency voltage/current coupled into the signal path; magnetic induction from transformer, motor, wiring, or other mains-fed equipment; common-impedance/ground-current coupling; intentional or acoustic tone | A tone near 50/60 Hz is not unique to mains or a ground loop. Exact frequency may differ from nominal and vary. Test against independent power reference, controlled unplug/substitution, or field/current measurements. |
| Integer-related low-frequency family | Candidate fundamental and measured peaks near integer multiples; relative harmonic levels and stability | Non-sinusoidal periodic interference, rectification/power-supply ripple, saturation/clipping, or a periodic source with harmonic waveform | Harmonicity supports a shared periodic process, not a named device. Missing harmonics may reflect filtering, transfer response, masking, or detection threshold. Do not infer mains solely from integer ratios. |
| Broad low-frequency buzz with dense harmonics | Band/peak distribution, modulation, crest and temporal behavior | Switching/rectifier circuitry, motor drive, lighting electronics, nonlinear signal path, or acoustic/mechanical vibration | “Buzz” is a perceptual label. More than one source can produce a similar comb. Inspect spectrogram and harmonics; confirm with source isolation. |
| Slowly or rapidly drifting narrow line | Frequency track, drift rate, interruptions and confidence | Electrical network frequency variation, oscillator/clock instability, motor speed change, Doppler/acoustic motion, sample-clock or playback-speed variation | Drift is measurable; its cause is not. Recording clocks, resampling and tape-speed variation also alter apparent pitch. ENF work requires a sufficiently resolved extracted track and matching reference records. |
| Switching-frequency-related line, sidebands, or broadband rise | Frequency bands, sideband spacing, amplitude modulation, time correlation | Switching converter/regulator emissions, digital clocks, display/LED drivers, RF interference coupled into analog electronics, or nonlinear demodulation | A spectral feature may be a switching fundamental, harmonic, alias, or intermodulation product. The source cannot be identified from frequency alone; switching rates can be load- and design-dependent. |
| USB / computer activity-correlated whine, ticks, or broadband change | Event timing and spectral change; correlation with a user-supplied activity log if available | USB ground/power noise, common-mode current, digital activity coupling into a converter or analog path, RF/EMI | USB is a path/context, not a spectral class. Recording alone does not identify USB as the route. Correlation during a controlled activity test is stronger but still needs an electrical-chain check. |
| Narrow high-frequency whistle or tone | Frequency, bandwidth, level, drift, duration and channel | Oscillator/clock leakage, switching electronics, feedback, acoustic resonance, or intentional signal | Could be above audibility but captured by the chain, or aliased into band. Inspect original sample rate and provenance; aliasing can make an out-of-band input appear at a lower frequency. |
| Periodic pulses, clicks, ticks, or bursts | Onset times, repetition interval, pulse width, spectrum and channel | Switching/load transitions, digital bus activity, relay/contact events, RF demodulation, clipping, packetized/interface activity, or physical contact noise | Repetition and temporal correlation are useful observations. “Digital” or “electrical” is not established by a click-like waveform. A transient can create broadband spectral energy through window spreading. |
| Broadband or colored noise floor | Band-limited RMS/PSD, slope, channel balance and changes over time | Thermal/electronic noise, microphone self-noise, preamp gain, ventilation/wind, tape/media noise, codec or processing artifacts, or many weak sources | A recording’s noise floor is the sum of source, room, microphone, preamp, converter, quantization and processing contributions. It cannot be assigned to one stage without a measurement design. |
| Intermodulation / beat products | Sum/difference candidates, modulation envelope, coherent relationships where measurable | Nonlinear device stage mixing two or more components; amplitude modulation; beating between nearby tones | Frequency coincidences alone do not prove nonlinearity. Establish source tones and inspect levels/phase/coherence; nonlinear loopback or controlled two-tone tests may help. |
| Channel-specific or coherent interference | Per-channel level, cross-spectrum/coherence and phase where defined | Shared supply/ground, stereo wiring geometry, channel-specific pickup, microphone placement, or duplicated/stereo processing | Coherence/phase can show relationship between recorded channels, not pinpoint the coupling route. Mono downmixing may cancel or reinforce components, so analyze original channels separately. |
| Acoustic/mechanical periodic sound | Recorded periodicity, harmonics, envelope and stereo/spatial difference | Fan, transformer core vibration, motor, HVAC, appliance, instrument, room resonance, or other sound pressure reaching the microphone | Audio-only measurements usually cannot determine whether a 50/60 Hz-related component entered electrically or acoustically. A microphone disconnected/terminated test and accelerometer/field probe can separate paths. |

## Mechanism families

Engineering literature describes several coupling routes that can coexist: galvanic/common-impedance coupling, capacitive coupling, magnetic induction, radiated electromagnetic coupling, and acoustic/mechanical transmission. Ground-loop discussions explain that current circulating through interconnected reference/shield paths can create a voltage in a shared signal impedance; magnetic induction from premises wiring can also create ground potential differences. Thus “ground loop” should not be used as a synonym for any mains-frequency sound.

Switch-mode supplies produce fast switching waveforms and can create conducted and radiated emissions; their spectral components and sidebands depend on topology, switching behavior, load and filtering. USB systems can carry power and ground references between host and peripheral, and USB isolation documentation addresses ground-potential/noise concerns. These sources establish plausible mechanisms, not a universal acoustic signature that identifies a device from a WAV file.

Sampling imposes a further limit: out-of-band input energy can fold into the digital passband (aliasing), and subsequent resampling or lossy encoding can change or erase diagnostic detail. A recorded line may therefore be an alias or artifact rather than an in-band physical oscillation at that apparent frequency.

## HumTrace terminology and reporting

Prefer:

- “A persistent component was measured at 59.94 Hz (±0.12 Hz under the stated window and estimator), at −48 dBFS RMS, in the left channel.”
- “A candidate harmonic family is present at approximately 60, 120 and 180 Hz; the detected peaks are consistent with a periodic process.”
- “This pattern may be consistent with mains-related coupling. The recording does not establish a ground loop or identify a device.”

Avoid categorical language such as “ground loop detected,” “USB noise found,” or “recording location identified” unless separately corroborated and the method has been validated for that inference. Report the estimator, window/frame length, overlap, sample rate, threshold, noise estimator, uncertainty, channel, and any preprocessing so another analyst can reproduce the measurement.

## Source notes

Accessed 2026-10-08.

1. Bill Whitlock and Jensen/Audio Engineering Society, [Ground Loops: The Rest of the Story](https://www.jensen-transformers.com/wp-content/uploads/2015/02/AES-Ground-Loops-Rest-of-Story-Whitlock-Fox-Generic-Version.pdf), AES Convention paper. Mechanism discussion for audio grounding and induced ground voltages.
2. JH Brandt Acoustics, [Zero Loop Area](https://jhbrandt.net/zero-loop), technical note compiling audio wiring/loop-area mechanisms and source papers. Practitioner engineering source; useful mechanism synthesis, not a diagnostic classifier.
3. Rane, [Sound System Interconnection](https://www.ranecommercial.com/legacy/note110.html), technical note on interconnection, ground current and audio hum.
4. Bel Fuse, [EMI Considerations for Switching Power Supplies](https://www.belfuse.com/resource-library/tech-paper/emi-considerations-for-switching-power-supplies), application note on conducted/radiated switching-supply emissions.
5. Texas Instruments, [USB Audio Isolation with Isolated USB 2.0 Redriver](https://www.ti.com/lit/pdf/slla601), application note on USB audio isolation and ground-loop/noise considerations.
6. Analog Devices, [Basics of Band-Limited Sampling and Aliasing](https://www.analog.com/en/resources/technical-articles/basics-of-bandlimited-sampling-and-aliasing.html), sampling and aliasing explanation.
7. SciPy, [`welch`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html) and [`periodogram`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.periodogram.html) reference documentation for PSD estimation and window/scaling parameters.
8. Texas Instruments, [SLLA580: USB isolation](https://www.ti.com/document-viewer/lit/html/SLLA580), discusses USB ground bounce and ground-potential differences in its application context.
