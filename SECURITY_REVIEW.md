# Security review (AI-assisted; not an independent audit)

Scope: code added/changed on this branch (analysis engine, SHA-256, CLI, GUI, CI) plus the existing WAV parser exercised by fuzzing. Method: manual diff review, ASan/UBSan test + 600-case mutation fuzz (`tests/fuzz_wav.cpp`), negative CLI tests. Not covered: TSan, formal fuzzing (libFuzzer/AFL), Qt internals, Windows.

| ID | Sev | Finding | Fix / status | Verification |
|---|---|---|---|---|
| S1 | Low | `write_file_safely` wrote to predictable `<out>.tmp`; a pre-planted symlink would be followed. | Fixed: symlink at temp path is refused. | `tests/test_core.cpp` regression |
| S2 | Med | Report could overwrite the input recording via `--json same.wav`. | Fixed: `fs::equivalent` check before any write. | `test_core`, `test_cli.py` |
| S3 | Med | Unbounded memory for very long files/huge frame requests. | Fixed: frame size 4..65536 power of two, 16M-sample window cap, `max_result_frames` cap (200k). | `test_core` (cap, bad sizes); 10-min file ~11 MB RSS |
| S4 | Low | Header lies about data length / truncated files. | Parser rejects or warns; no crash under ASan/UBSan. | `test_core`, `fuzz_wav` (209 accepted / 391 rejected, 0 faults) |
| S5 | Low | Signal handler: only a lock-free atomic store. | Reviewed, OK. | code review |
| S6 | Low (open) | GUI spectrum re-reads the file after analysis; if it changed meanwhile, the plot reflects different bytes than the report hash. | Open, accepted for beta; re-hash on selection would fix. | none |
| S7 | Info (open) | JSON includes the absolute input path (may leak directory/user names). | Open; documented behavior of schema 1.x. | none |
| S8 | Low (open) | CI actions pinned by tag, not commit SHA; Windows job uses third-party `msys2/setup-msys2@v2` (pre-existing). | Open; permissions limited to `contents: read` except the publish job. | workflow review |
| S9 | Info | Qt is LGPL/GPL; not bundled in release archives. | Flagged to owner in THIRD-PARTY-NOTICES. | n/a |
| S10 | Info | SHA-256 is an original implementation. | Checked against 4 published vectors incl. 1M×'a'. | `test_core` |
| S11 | Med | `--compare --json` could target any compared input, not just the first. | Fixed: every input checked with `fs::equivalent` before writing; GUI export refuses input paths. | `test_cli.py` (second input), `gui_smoke` |
| S12 | Low | Tracker/display memory scaled with peaks and length. | Bounded: <=64 peaks/frame, <=1024 active / 100k tracks (defaults 64 / 4096), waveform <=65536 buckets, spectrogram <=8192x2048 (defaults 4096, 1024x256); out-of-range options rejected. 10-min stereo 48 kHz: 13 MB RSS, 14.2 s (baseline 14.4 s). | `test_tracking` |
| S13 | Info | Tone tracks near n x 50/60 Hz could be read as a source claim. | Labelled `near_mains_multiple` (proximity only, n<=10); report and comparison interpretation notes state no source claim. | `test_report`, `test_cli.py` |

No secrets were found in code, fixtures or workflows. No shell or SQL strings are built from input.
