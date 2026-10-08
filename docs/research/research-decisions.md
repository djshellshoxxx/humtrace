# HumTrace open-source dependency decisions

**Date:** 2026-10-08  
**Status:** Research recommendations for implementation review. These are not a legal opinion or a final dependency lock.

## Decisions to carry into implementation

1. **Keep the analysis core independent of JUCE.** The current architecture already makes this an explicit boundary. JUCE's `juce_dsp` can inform API and DSP conventions, but its modules are AGPLv3/commercial dual-licensed and its FFT is described upstream as a low-footprint, not speed-tuned implementation. Consider JUCE only if the project separately chooses its UI/audio framework and reviews the applicable JUCE 9 terms. [1]

2. **Keep the existing radix-2 FFT until a benchmark justifies replacement.** The prototype only accepts power-of-two analysis windows. Implement an adapter boundary now only if doing so remains small; otherwise benchmark before adding indirection. If the implementation becomes a bottleneck or non-power-of-two lengths are needed, evaluate KissFFT first because its BSD-3-Clause license and simple C API align with a small native dependency. Validate window/FFT scaling and one-sided normalization against HumTrace's existing analytical fixtures and SciPy. [2][3][4]

3. **Do not choose FFTW as the default distributable backend.** FFTW has strong performance and portability, but upstream distributes it under GPL and separately offers a commercial license. Reconsider only with an explicit product licensing decision or an appropriate commercial license. [5]

4. **Do not adopt PFFFT before target-CPU and license review.** It is a useful performance comparison, especially for SIMD, but the evaluated repository is a fork and its license includes multiple copyright/derived-material notices. Benchmark the exact pinned commit and inspect every bundled source/license before integration. [6]

5. **Defer a decoder dependency for the current scope; evaluate libsndfile for format expansion.** HumTrace already reads basic RIFF/WAVE PCM and float. First harden parser tests and compare dr_wav against the exact unsupported cases. For broader supported audio formats, libsndfile is the leading candidate because it is an audio-focused C library with CMake builds and LGPL 2.1-or-later or LGPL 3 licensing choices. Evaluate actual build options, external codec libraries, binaries, and relinking/source obligations before packaging. [7][8]

6. **Treat dr_wav as a focused WAV alternative, not an automatic upgrade.** Its single-file integration and dual license alternatives (Unlicense or MIT No Attribution) are attractive, but adoption still requires testing metadata handling, malformed-file behavior, security/update process, and exact vendored license choice. [9]

7. **Keep FFmpeg optional and configuration-pinned if broad codec support is later required.** FFmpeg has the widest decoding role among these candidates, while licensing changes with enabled GPL components and build configuration. Do not rely on whatever FFmpeg happens to be installed on a user's machine without recording capabilities and settings. It can be useful as a developer-side fixture converter/cross-check, provided test provenance records its version and command line. [10][11]

8. **Use SoX only as an optional developer cross-check.** Its CLI and effects can help generate or inspect fixtures, but it adds no needful runtime capability to the present product. The project lists GPLv2 and LGPLv2 components, so inspect component-specific terms before embedding or redistributing. [12]

9. **Use SciPy as the primary independent numerical test oracle; use librosa for exploration.** SciPy provides reference FFT/signal methods under BSD-3-Clause and can verify FFT normalization, windows, STFT, Welch, and peak behavior. Keep SciPy out of the C++ runtime. librosa's high-level APIs can speed research, but tests must override default mono conversion/resampling (`mono=False`, `sr=None`) and pin its audio backend/dependencies to avoid comparing different samples. [13][14][15]

## Adoption vs. study boundary

| Action | Suitable candidates | Conditions |
|---|---|---|
| Adopt as C++ runtime dependency | KissFFT for FFT; libsndfile for expanded decoding; dr_wav for focused WAV decoding | Pin version/commit, add license notices, test relevant platforms and edge cases, and preserve HumTrace-owned DSP/measurement tests. Choose one decoder path rather than layering overlapping parsers without a specific reason. |
| Use as optional developer tool | SciPy, librosa, SoX, FFmpeg | Record versions, arguments, transforms and expected tolerances in fixtures. Keep generated files and test scripts reproducible. |
| Study implementation and documentation | JUCE DSP, FFTW, PFFFT, all decoder projects | Learn API, algorithm and performance tradeoffs; do not copy implementation code unless the exact source license and notice obligations are audited and accepted. |
| Avoid for current runtime | FFTW default build; JUCE as a DSP-core dependency; FFmpeg as an unspecified system dependency; Python packages | Their licensing, framework coupling, build breadth, or runtime footprint does not currently solve a measured HumTrace requirement. |

## Required checks before adding any dependency

- Record exact upstream URL, release/tag/commit, hash, license expression, and any bundled/transitive component licenses in the dependency inventory.
- Confirm the chosen build configuration, static/dynamic linking model, distribution method, and notices/source/relinking requirements for the actual artifacts.
- Test on each claimed platform with the dependency disabled/enabled as intended; do not describe a target as verified from source-level portability alone.
- Keep primary analysis settings explicit: channel count/order, sample rate, sample conversion, frame size, hop, window, scaling, PSD/amplitude units, decoder transformations, and dependency versions.
- Compare all backend outputs against deterministic synthetic signals and stable stored fixtures before changing expected values.

## Sources

All sources were accessed 2026-10-08. See [existing open-source building blocks](existing-open-source.md) for the detailed comparison and full source list.

1. [JUCE LICENSE.md](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md), [JUCE FFT header](https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/frequency/juce_FFT.h).
2. [KissFFT README](https://github.com/mborgerding/kissfft), [BSD-3-Clause license](https://github.com/mborgerding/kissfft/blob/master/LICENSES/BSD-3-Clause).
3. [SciPy FFT tutorial](https://docs.scipy.org/doc/scipy/tutorial/fft.html).
4. [HumTrace DSP algorithms](dsp-algorithms.md), [architecture overview](../architecture/overview.md).
5. [FFTW license and copyright](https://fftw.org/doc/License-and-Copyright.html).
6. [PFFFT fork README](https://github.com/marton78/pffft/blob/master/README.md), [license](https://github.com/marton78/pffft/blob/master/LICENSE.txt).
7. [HumTrace current prototype README](../../README.md), [libsndfile documentation](https://github.com/libsndfile/libsndfile/blob/master/docs/index.md).
8. [libsndfile CMake repository notes](https://github.com/libsndfile/libsndfile).
9. [dr_libs LICENSE](https://github.com/mackron/dr_libs/blob/master/LICENSE), [dr_wav header](https://github.com/mackron/dr_libs/blob/master/dr_wav.h).
10. [FFmpeg LICENSE.md](https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md).
11. [FFmpeg legal guide](https://ffmpeg.org/legal.html).
12. [SoX project page](https://sourceforge.net/projects/sox/).
13. [SciPy repository](https://github.com/scipy/scipy), [SciPy FFT tutorial](https://docs.scipy.org/doc/scipy/tutorial/fft.html).
14. [librosa LICENSE](https://github.com/librosa/librosa/blob/main/LICENSE.md), [`librosa.load` API](https://librosa.org/doc/latest/generated/librosa.load.html).
15. [HumTrace DSP algorithm research](dsp-algorithms.md).
