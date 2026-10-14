# QA report (beta v0.1.0)

Environment: Linux x86-64, g++ 13, Qt 6.4.2, offscreen Qt platform. Everything below was run by the project tooling in this repository; nothing here was tested on Windows hardware, with a screen reader, or on real field recordings.

## Test plan and results

| Area | Method | Result |
|---|---|---|
| Unit/integration | `make test`, `ctest` (15 suites incl. WAV mutation fuzz of 20,000 inputs, offscreen GUI tests) | pass |
| Sanitizers | core suite and 30 s core fuzz under ASan+UBSan (the Qt/GUI tests are not sanitized) | clean |
| Format matrix | `tools/qa_run.py`: 6 encodings x 1-3 channels x plain/extensible x 8/48 kHz = 72 files; each checked for exit code, source SHA unchanged, report SHA equal, per-channel tone at the injected frequency, schema validity | 90/90 (the matrix now includes the output-safety, comparison and cancellation checks) |
| Corrupt files | 9 hostile inputs (empty, garbage, truncated, bad sizes, zero channels/rate, unsupported codec, too short) | clean exit 1, message, no output, source intact |
| Output safety | unwritable destination, overwrite, output == input | no partial/temp files; input never overwritten |
| Determinism | 3 runs, byte-identical JSON | pass |
| Comparison | near tones matched, others unmatched, no alignment/resampling recorded, bad tolerance rejected | pass |
| Long files | 1 h vs 3 h at 8 kHz: peak RSS 33 MB vs 82 MB (grows with duration, about 25 MB/h/channel; bounded by the 512 MiB result cap); Ctrl+C cancel latency on a 3 h file under 1 s, no output | pass |
| Measurement accuracy | `tools/validate_synthetic.py` (36 tone fixtures, 2 negative fixtures) | 36/36 detected; max error 0.03 Hz, 0.26 dB; no mains support in noise/silence |
| GUI behavior | analyze stereo, per-channel findings, finding selection, keyboard navigation, cancel, invalid file, stale settings, export redaction, bad open keeps results, literal `%`/`<` in names | pass |
| GUI visuals | screenshots reviewed: complete, linear+grayscale, zoomed channel 2, narrow 1024x680, stale, error | reviewed; label clipping fixed |
| Randomized settings matrix | see below | 0 errors after fixes |

## Settings-matrix fuzz (30 s runs)

Every step re-randomizes all analysis settings (including deliberately invalid values), and the input recording (rate, channels, encoding, tones, noise), from a fresh seed (OS entropy + clock + counter); the exercised feature rotates (analyze + determinism, compare + export, cancel + re-run; GUI: export, navigation, finding selection, re-analysis, cancel). Each error prints a `--replay` seed. Latest runs: core 868 steps (326 verified analyses, 116 invalid-settings rejections, 159 cancellations), GUI 275 steps (220 analyses), 0 errors. The fuzz first reported accepted-invalid-settings when validation was disabled (mutation check), confirming it detects that class of bug.

## Bugs found and fixed

See [code audit](../audit/code-audit.md): 18 fixed items, including input-overwrite, DC-offset mis-detection, and an unanalyzed final hop.

## Not tested / residual risk

Windows runtime, packaged-app install on a clean VM, Narrator/AT-SPI behavior, high-DPI scaling, real recordings, codecs other than WAV, files over several hours. The beta checklist reflects these gaps.
