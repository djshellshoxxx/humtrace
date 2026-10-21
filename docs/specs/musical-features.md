# Musical labels and CSV export (v0.2)

| Module | Contract |
|---|---|
| `music.hpp` | `nearest_note(hz, a4)` → MIDI number, name (`B1`), equal-tempered Hz, signed cents; rejects non-positive/non-finite input. `format_note` → `B1 -49c`. |
| `export.hpp` | `serialize_tracks_csv(report, a4)`: one row per track — channel, id, start/end s, median Hz, note, cents, median dBFS, drift Hz/s, observed/missing frames. |
| CLI | `--csv <file>` (atomic write, refuses overwrite without `--force`), `--a4 <Hz>` 400–480. |
| UI | Findings inspector line and frame-table label show the nearest note. |

Notes are labels for frequencies, never evidence of musical content or source. The JSON schema is unchanged (1.2). Tests: `tests/test_music.cpp`, `check_csv_and_notes` in `tests/test_cli.py`.

Deferred: harmonic-series/interval view, MIDI export, user tunings beyond A4.
