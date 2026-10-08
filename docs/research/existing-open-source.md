# Existing open-source building blocks for HumTrace

**Reviewed:** 2026-10-08  
**Scope:** Upstream capabilities, licensing, platform/dependency observations, and the boundary between adopting code and studying it. License observations describe upstream files as reviewed on the date above; they are not legal advice. Recheck the exact release, configuration, and transitive dependencies before packaging.

## Summary

HumTrace already has a small C++20 WAV decoder and radix-2 FFT. These projects offer useful alternatives, but there is no reason to replace the working prototype before measuring a concrete gap. For a permissive native dependency, KissFFT is the clearest first FFT candidate. For broader audio-file support, libsndfile is a mature C library with an LGPL choice; dr_wav is a small single-header WAV option under either public-domain dedication or MIT terms. SciPy and librosa are strongest as independent analysis references and test oracles, not as C++ runtime dependencies. FFTW, JUCE, FFmpeg, and SoX each bring licensing or packaging choices that should be made deliberately.

## Project comparison

| Project | Useful HumTrace capability | Upstream license / platform notes | Adopt vs. study |
|---|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | `juce_dsp` includes FFT and common DSP utilities; the framework also provides audio-format and GUI modules. | JUCE 9 modules are dual-licensed under AGPLv3 and the JUCE commercial licence. Its FFT header describes the implementation as simple/low-footprint rather than speed-tuned. JUCE also publishes an SPDX SBOM for bundled third-party components. The repository supports multiple desktop and mobile targets; exact target support and terms depend on the JUCE version and product use. [1][2] | Study API shape and DSP conventions. Adopt only if HumTrace chooses JUCE for the app and its licensing is acceptable; keep `dsp` independent of JUCE per the current architecture. Do not select it solely to obtain an FFT. |
| [FFTW](https://www.fftw.org/) | Fast real/complex FFTs, arbitrary transform sizes, SIMD options, and threaded transforms. | Official site identifies FFTW 3.3.11 and says it is portable to platforms with a C compiler. The upstream license is GPL; the project also says a non-free license may be purchased from MIT. [3][4] | Study transform/scaling behavior and benchmark methodology. For a permissively licensed distributable HumTrace build, do not link the GPL build without an explicit licensing decision. |
| [KissFFT](https://github.com/mborgerding/kissfft) | Compact C FFT with real-transform support, straightforward API, and build-system support; useful as an FFT backend behind HumTrace's own interface. | The repository contains a BSD-3-Clause license. Its README warns that scaling and consistent compile-time definitions matter. Confirm the selected source files and any optional components at the pinned revision. [5][6] | Best initial candidate for a small, permissive FFT dependency if benchmarking shows the in-tree implementation is insufficient. Retain HumTrace-owned tests for scaling, bin mapping, and determinism. |
| [PFFFT, marton78 fork](https://github.com/marton78/pffft) | SIMD-oriented FFT and related transform operations; repository documents SIMD options including SSE, AVX, NEON, Altivec, and WebAssembly SIMD. | This is a maintained fork with changes beyond Julien Pommier's original. Its license file describes a BSD-style license and includes FFTPACK-derived material with separate attribution/license discussion; inspect the complete file and pinned tree before vendoring. SIMD availability and build flags are CPU/compiler dependent. [7][8] | Study and benchmark against KissFFT on supported targets. Adopt only after deciding supported CPU baselines, validating each SIMD path, and auditing all included files. |
| [libsndfile](https://github.com/libsndfile/libsndfile) | Native C library to read/write sampled audio files; supports a broad set of common PCM/float audio containers and codecs, subject to build options and external codec libraries. | Upstream offers a choice of LGPL-2.1 or LGPL-3.0. It provides Autotools and CMake builds and documents Windows, Linux, Apple platforms, and Android builds; external codec support and binary names vary by build/platform. [9][10] | Strong candidate when HumTrace expands beyond PCM/float WAV. Hide it behind `audio_io`, preserve original sample rate/channel metadata, and audit enabled codecs and redistribution obligations for the chosen build. |
| [dr_wav in dr_libs](https://github.com/mackron/dr_libs) | Single-file C/C++ WAV decoder, designed for embedding; related single-file libraries cover FLAC and MP3. | The project license offers two alternatives: public-domain dedication (Unlicense) or MIT No Attribution. A single-header distribution reduces integration steps but still needs pinned-version review, security updates, and conformance tests. [11][12] | Candidate for a focused WAV reader if current decoder limitations justify replacement. For current PCM/float RIFF/WAVE scope, compare malformed-file handling and metadata behavior before deciding. |
| [FFmpeg](https://github.com/FFmpeg/FFmpeg) | Broad demuxing/decoding through `libavformat` and `libavcodec`; useful when supporting many compressed recording formats. | Most code is LGPL-2.1-or-later. Optional components selected with `--enable-gpl` change the overall FFmpeg build to GPL; external libraries and build flags affect the license. Its official legal guide documents corresponding binary/source notices and relinking requirements for LGPL distribution. Platform binaries are commonly built by third parties with differing configure options. [13][14] | Study as an external conversion/test oracle or consider an optional decoder backend if format breadth becomes a release requirement. Avoid silently depending on arbitrary system FFmpeg builds; pin and document configuration and licensing. |
| [SoX](https://sourceforge.net/projects/sox/) | Command-line sound conversion, inspection, and effects; useful for independent fixture generation and cross-checks. | The upstream project page lists GPLv2 and LGPLv2 licensing, and its latest listed update is 2024-11-28. The executable and libSoX are distinct components; inspect source-level notices before embedding any code. [15] | Use as an optional developer-side cross-check, not a core library dependency. Do not bundle the CLI merely for HumTrace analysis. |
| [SciPy](https://github.com/scipy/scipy) | `scipy.fft`, `scipy.signal` provide independent reference implementations and tools for FFT, Welch PSD, STFT, coherence, windows, and peak checks. | SciPy is BSD-3-Clause and built around NumPy arrays. Its FFT docs describe transform normalization and one-sided real-input output; using it in the app would add a Python/NumPy runtime. [16][17] | Use in offline test scripts/notebooks as a numerical oracle and for corpus exploration. Do not make the C++ app depend on Python/SciPy. Keep fixtures and expected tolerances reproducible. |
| [librosa](https://github.com/librosa/librosa) | High-level audio/music analysis references, framing, STFT and feature extraction. | librosa is ISC-licensed. Its `load` API may resample and convert to mono by default unless configured otherwise, and file loading uses optional backend dependencies. Those defaults can violate forensic measurement requirements if not explicitly controlled. [18][19] | Study algorithms and prototype analysis only. Use `sr=None` and `mono=False` when building comparisons, and record backend/transforms. Avoid using default output as a ground-truth oracle. |

## Practical evaluation notes

### FFT backends

The current in-tree radix-2 FFT is adequate for the prototype's power-of-two windows and keeps builds simple. Keep a backend-neutral HumTrace contract, then benchmark the same representative transforms with the in-tree implementation, KissFFT, and (if useful) PFFFT. Compare numerical error, one-sided bin conventions, scaling, odd/even lengths, memory ownership, build friction, and performance across Windows and Linux. A faster library does not fix frame selection, windowing, PSD normalization, or measurement validation.

FFTW is technically capable, but its GPL/commercial-license fork makes it a poor default for a broadly distributable application. JUCE can be a sensible framework choice for a future desktop/plugin shell, but using a JUCE FFT would couple the core or introduce a framework-specific licensing decision without a demonstrated need.

### Audio decoding

For the current WAV-only target, preserve and harden the existing parser or compare it with dr_wav; test chunk ordering, padding, extensible format/channel masks, truncated chunks, integer valid bits, and float edge cases. When adding FLAC/AIFF/other formats, libsndfile is the narrower audio-focused general-purpose candidate. FFmpeg offers wider codec coverage but is a larger configuration and compliance surface. Whichever decoder is chosen, HumTrace should disable implicit downmix/resampling, retain format metadata, and test that channel order and sample values match the decoder contract.

### Independent references

Use SciPy's documented implementations to generate known expected results for windowing, real FFTs, STFT, Welch PSD, peak properties, and coherence. Keep the C++ tests independent by storing deterministic input fixtures and expected values with stated tolerances. librosa can accelerate exploratory analysis, but defaults such as resampling/mono conversion need to be overridden so test comparisons have the same sample data and channel layout.

## Sources

All links below are first-party project repositories or official project documentation and were accessed 2026-10-08.

1. JUCE, [LICENSE.md](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md).
2. JUCE, [`juce_dsp` FFT header](https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/frequency/juce_FFT.h) and [repository](https://github.com/juce-framework/JUCE).
3. FFTW, [home page/features](https://www.fftw.org/).
4. FFTW, [License and Copyright](https://fftw.org/doc/License-and-Copyright.html).
5. KissFFT, [repository README](https://github.com/mborgerding/kissfft).
6. KissFFT, [BSD-3-Clause license](https://github.com/mborgerding/kissfft/blob/master/LICENSES/BSD-3-Clause).
7. PFFFT fork, [README](https://github.com/marton78/pffft/blob/master/README.md).
8. PFFFT fork, [LICENSE.txt](https://github.com/marton78/pffft/blob/master/LICENSE.txt).
9. libsndfile, [documentation index and supported platforms/license](https://github.com/libsndfile/libsndfile/blob/master/docs/index.md).
10. libsndfile, [repository and CMake build notes](https://github.com/libsndfile/libsndfile).
11. dr_libs, [repository and included single-file libraries](https://github.com/mackron/dr_libs).
12. dr_libs, [LICENSE](https://github.com/mackron/dr_libs/blob/master/LICENSE).
13. FFmpeg, [LICENSE.md](https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md).
14. FFmpeg, [official License and Legal Considerations](https://ffmpeg.org/legal.html).
15. SoX, [project page, license, and release activity](https://sourceforge.net/projects/sox/).
16. SciPy, [repository and BSD-3-Clause metadata](https://github.com/scipy/scipy).
17. SciPy, [FFT tutorial](https://docs.scipy.org/doc/scipy/tutorial/fft.html).
18. librosa, [LICENSE.md](https://github.com/librosa/librosa/blob/main/LICENSE.md).
19. librosa, [`librosa.load` API](https://librosa.org/doc/latest/generated/librosa.load.html).
