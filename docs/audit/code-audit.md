# Code audit (tracker/hash/pipeline increment)

Scope: all of `src/`, `include/`, `tests/`, Makefile. Verified with `make test` and ASan/UBSan (`-Werror -Wconversion`).

| # | Finding | Severity | Status |
|---|---|---|---|
| 1 | `main.cpp` mixed I/O, windowing, and analysis; untestable | Medium | Fixed: moved to `pipeline` module |
| 2 | No input hash in reports | Medium | Fixed: streaming SHA-256, schema 1.1 |
| 3 | WAVE_FORMAT_EXTENSIBLE rejected | Low | Fixed: PCM/float sub-formats, full valid bits only |
| 4 | Tracks keep every observation in memory (O(frames) per channel) | Medium | Open: acceptable for prototype; bound with incremental report writing (long-file spec) |
| 5 | Timeline holds all `InterferenceFrame`s in memory, as does JSON serialization | Medium | Open (documented in README) |
| 6 | Tracking uses only the strongest peak per frame; no multi-peak finder, no split/merge lineage | Medium | Open: next step per temporal-tracking spec |
| 7 | No cancellation, no RF64, no fuzzing of the WAV parser | Medium | Open: long-file and audio-input specs |
| 8 | CLI hashes the file then re-reads it (two passes) | Low | Accepted: keeps modules independent |
| 9 | `analyze_interference_timeline` copies each frame block | Low | Accepted: pipeline passes one window per call |
| 10 | Report schema validation in CLI test is skipped when `jsonschema` is absent | Low | Accepted |

No memory-safety or undefined-behavior findings from sanitizer runs. Wording rules (no source attribution, no "persistent" label) are preserved in code, report, and docs.
