# Release 0.2.4 stress QA

## Environment and scope

Linux x86-64, GCC, Python 3. A fresh `build-qa-final` directory was force-built with two jobs. JSON Schema validation was required with `PYTHONPATH=/workspace/humtrace-tools HUMTRACE_REQUIRE_JSONSCHEMA=1`. RSS values are Linux child-process peak resident set from `wait4`; the bounded streaming runner also reports per-analysis RSS under `RLIMIT_AS=96 MiB`.

Generated tone and noise fixtures are synthetic unit fixtures with known truth. They do not establish performance on field recordings. Real-recording cross-checks require an available WAV corpus and the optional `numpy`/`soundfile` packages.

## Results

| Command | Result | Elapsed | Peak RSS / detail |
|---|---:|---:|---|
| `make -B BUILD_DIR=build-qa-final -j2 test` | pass | 26.36 s | 251,848 KiB max child during fresh compile; includes 14 C++ suites, WAV mutation fuzz, CLI, streaming, CLI paging, detector CLI, package and license tests |
| `python3 tools/qa_run.py build-qa-final/humtrace --markdown /tmp/humtrace-qa-0.2.4-final.md` | 97/97 pass | 44.86 s | 28,328 KiB max child; 1 h and 3 h regular CLI runs each used 28 MiB and exited 0 |
| `python3 tools/validate_synthetic.py build-qa-final/humtrace --markdown /tmp/humtrace-synthetic-0.2.4-final.md` | 36/36 tone fixtures detected | 7.95 s | 58,828 KiB max child; max frequency error 0.0312 Hz, max level error 0.257 dB |
| `python3 tools/validate_streaming_memory.py build-qa-final/humtrace` | pass | 29.73 s, including fixture generation | 13,440 KiB max child; per-analysis RSS 9.00 MiB at 1 h and 9.36 MiB at 3 h |

The streaming 1 h and 3 h inputs produced 14,062 and 42,187 complete measurements and reports of 108,265,215 and 324,865,721 bytes. The address-space limit was 96 MiB; streaming RSS increased by 0.36 MiB across the threefold duration increase. Input hashes were unchanged, and scratch and temporary output files were removed. A SIGINT during the 3 h analysis returned exit 130 in 0.001 s without publishing a report or leaving scratch files. The regular CLI with automatic disk-backed paging also stayed flat at 28 MiB in the paired 1 h/3 h matrix, and both child processes returned exit 0.

The CLI matrix includes all six PCM/IEEE-float encodings, one to three channels, plain and extensible WAV headers, 8 kHz and 48 kHz, hostile header/alignment/sample cases, deterministic JSON, UTF-8 paths, atomic replacement, source immutability, and cancellation. Streaming report parity covers five stereo signal types plus close two-tone and crossing two-tone single-channel fixtures with one-frame tracks retained and explicit ambiguity references. The synthetic negative fixtures reported zero mains-harmonic support in white noise and digital silence; one short noise track was present but had fewer than five frames.

## Held-out recordings

No WAV recordings were present in the checkout or the documented LibreOffice system path (`/usr/lib/libreoffice/share/gallery/sounds`), and the manifest states its Google sample files are not redistributed. The command `python3 tools/real_recordings_check.py build-qa-final/humtrace /workspace/recordings` exits 1 with `real-recordings directory does not exist: /workspace/recordings`. No real-recording accuracy result is claimed. The checker now fails clearly for a missing or empty directory instead of reporting a successful zero-file run.
