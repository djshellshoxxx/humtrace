# HumTrace user guide (beta v0.1)

HumTrace measures narrowband tones and 50/60 Hz harmonic patterns in WAV recordings. It reports **measurements**, not causes.

## Open and analyze

1. **Open Recording** (Ctrl+O, or drag a file onto the window). Supported: WAV, PCM 8/16/24/32-bit and IEEE float 32/64-bit (including WAVE_FORMAT_EXTENSIBLE). The file is opened read-only and never modified.
2. **Analyze** (Ctrl+R). Hashing then analysis run in the background; the window stays responsive. **Cancel** (Ctrl+.) stops promptly and discards partial results.
3. Each channel is analyzed independently. Nothing is mixed, resampled, or aligned.

## Reading the result

- **Waveform overview**: min/max envelope per channel (selected channel solid, others dashed).
- **Spectrogram**: time x frequency, labeled in dBFS peak amplitude. Log or linear frequency, default or grayscale palette, floor/ceiling — display-only; they never change analysis.
- **Spectrum / Spectrum table**: the frame under the cursor. The plot is reduced to log-frequency rows (maximum bin per row); the table gives exact peak and 50/60 Hz harmonic-bin values.
- **Findings**: one row per measured track (channel, start/end, median frequency, median peak level, coverage, drift). The default order is by channel and start time; click a header to sort by that column. Selecting a row moves the cursor and selection and fills the inspector (measurements, harmonic evidence with thresholds, settings, limitations).

Levels are peak-amplitude dBFS from decoded samples, not SPL or voltage.

## Recent files

The left rail lists recently opened recordings (stored locally in the application settings); **Clear recent list** removes them.

## Settings

**Settings** (Ctrl+,) are analysis settings; changing them marks results stale until you re-run Analyze. Tracks seen in fewer than *Minimum frames per track* (default 3) frames are not listed, to suppress noise flicker; the report records how many were left out (`suppressed_tracks`) and adds a warning. This is not a validated persistence rule. The app lists clipping (samples at full scale) and left-out tracks as warnings in the export preview and the report.

## Export and compare

- **Export** (Ctrl+E) shows a preview (schema version, input SHA-256, track count). The default stores the file name only, not the directory. Saving is atomic: a failed export never leaves a partial file.
- **Compare** (Ctrl+Shift+C) analyzes a second recording and lists matched and unmatched tracks. No time alignment or resampling is applied (shown in a banner and recorded in the exported comparison). Unmatched tracks stay unmatched; absence is not scored as zero.
- CLI: `humtrace file.wav --json report.json [--quiet] [--full-path] [--force]`, `humtrace compare a.wav b.wav --tolerance 1.0 --map 1:2,2:1 --json c.json`. Reports store only the input file name unless `--full-path` is given; an existing output file is refused unless `--force`; an output may never be the input. Ctrl+C cancels. Both CLI and desktop accept UTF-8 paths on Windows.

## Reproducible detector configuration

The desktop Settings dialog and CLI expose frame/hop, spectrum and Welch detrending,
peak frequency band/separation, baseline guard/radius, thresholds, and bounded nominal
harmonic search. Only the Hann window is supported. For example:

`humtrace file.wav --frame-size 4096 --hop-size 1024 --band-min-hz 20 --band-max-hz 1000 --harmonic-tolerance-hz 2 --json report.json`

`--keep-dc` keeps the frame mean in the amplitude spectrum; `--psd-remove-mean`
subtracts each Welch segment mean. The defaults are independent. Effective choices
are stored in schema 1.3 reports. Search tolerance is a configured neighborhood, not
an error bar or probability. A null uncertainty means no calibrated uncertainty model
is available. Use the reported bin spacing, Hann width, boundary and competition flags
when inspecting a peak. Candidate split/merge links mark unresolved associations;
track IDs are local to a channel/run and can reference suppressed short segments.

For long reports use `--stream --json report.json`. The desktop chooses disk-backed
storage for long/dense analyses automatically. Exports retain complete measurements,
including intervals with missing observations; storage never invents gap samples.

## Keyboard

Ctrl+Down / Ctrl+Up next/previous finding. In a chart: Left/Right move the cursor by one analysis hop (Ctrl = 10 hops; Shift extends the selection), +/- zoom, 0 reset zoom, Home/End, Esc clears the selection. The mouse wheel zooms, horizontal scroll pans, dragging selects. **View > High-contrast theme** switches the theme. F1 shows this list and the limitations.

## Limits

See [known limitations](limitations.md). A frequency pattern does not identify a device, ground loop, location, recording date, or authenticity.
