# Code audit

Scope: all of `src/`, `include/`, `app/`, `tests/`, build files. Method: own review, an independent unanchored review by a separate agent, sanitizer runs (ASan/UBSan, `-Werror -Wconversion`), and the randomized settings fuzzers.

## Fixed

| # | Finding | Severity | Regression test |
|---|---|---|---|
| 1 | `--json` (CLI, GUI export, comparison export) could overwrite the input recording | High (data loss) | `test_session`, `test_cli.py`, `test_gui` |
| 2 | DC offset leaked into bin 1 and became the strongest "non-DC" peak / a bogus ~1 Hz track | High (wrong results) | `test_spectrum` |
| 3 | Final partial hop never analyzed; waveform envelope time-stretched (up to ~35%) | High (missed audio, wrong time axis) | `test_pipeline` |
| 4 | Invalid pipeline settings (NaN, negative, zero counts) accepted or failed deep with a vague error | Medium | `settings_fuzz` (validated by removing the check: detected) |
| 5 | Sample rates below 8 Hz threw `invalid_argument` | Low | `test_pipeline` |
| 6 | Odd-sized final data chunk without pad byte rejected; short trailing bytes rejected | Low-Medium | `test_wav` |
| 7 | Duplicate channel-map entries double-counted matches | Low-Medium | `test_compare` |
| 8 | Stale channel list could index `channel_results` out of range (file replaced between open and analyze) | Medium (UB) | bounds guards; list rebuilt from the result |
| 9 | Failed open (bad/missing file, stale recent entry, drop) discarded finished results | Medium | `test_gui` |
| 10 | `QString::arg` chaining corrupted names containing `%N`; labels rendered rich text | Low | `test_gui` |
| 11 | QThread deleted via `deleteLater` while possibly still running | Medium (crash risk) | worker released only after `wait()` |
| 12 | Non-UTF-8 paths produced invalid JSON | Low | `test_report` |
| 13 | `max_gap_frames = SIZE_MAX` overflow dropped every track | Low | `test_tracker` |
| 14 | `--tolerance 1abc` accepted as 1 | Low | CLI |
| 15 | Non-ASCII file names failed on Windows (narrow paths); file changed during analysis went undetected | Medium | `test_session` |
| 16 | High-contrast theme did not recolor the charts | Low | theme tokens shared by all charts |
| 17 | Committed a CMake build directory (broke CI) | CI | removed, ignored |
| 18 | `normalize_db` returned NaN when floor equals ceiling | Low | guard |

## Open

| Item | Severity |
|---|---|
| Results (frames, tracks, spectrogram) are held in memory and the JSON is built in one string; capped by `max_result_bytes` (default 512 MiB), not streamed | Medium |
| Tracks follow prominent peaks only; no split/merge lineage; persistence rule not validated | Medium |
| CLI on Windows needs ASCII paths (`wmain` not used) | Low |
| `compare` analyzes both files before rejecting a bad `--tolerance` | Low |
| Windows build/package unverified on a clean machine; screen-reader behavior unverified | Medium (release gate) |
| Table selection does not follow a channel change made from the channel list | Low |
