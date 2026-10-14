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

## Settings

**Settings** (Ctrl+,) are analysis settings; changing them marks results stale until you re-run Analyze. Tracks seen in fewer than *Minimum frames per track* (default 3) frames are hidden to suppress noise flicker. This is not a validated persistence rule.

## Export and compare

- **Export** (Ctrl+E) shows a preview (schema version, input SHA-256, track count). The default stores the file name only, not the directory. Saving is atomic: a failed export never leaves a partial file.
- **Compare** (Ctrl+Shift+C) analyzes a second recording and lists matched and unmatched tracks. No time alignment or resampling is applied (shown in a banner and recorded in the exported comparison). Unmatched tracks stay unmatched; absence is not scored as zero.
- CLI: `humtrace file.wav --json report.json`, `humtrace compare a.wav b.wav --tolerance 1.0 --json c.json`. Ctrl+C cancels. On Windows the CLI expects ASCII paths; use the GUI for other names.

## Keyboard

Ctrl+Down / Ctrl+Up next/previous finding. In a chart: Left/Right cursor (Ctrl = 10x, Shift extends the selection), +/- zoom, 0 reset zoom, Home/End, Esc clears the selection. The mouse wheel zooms, horizontal scroll pans, dragging selects. **View > High-contrast theme** switches the theme. F1 shows this list and the limitations.

## Limits

See [known limitations](limitations.md). A frequency pattern does not identify a device, ground loop, location, recording date, or authenticity.
