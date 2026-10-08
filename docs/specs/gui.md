# HumTrace desktop GUI specification

**Status:** v0.1 interaction and layout specification. Visual implementation and toolkit selection are not complete.

## Main window

Use a three-column analysis workspace with a compact top command bar and a shared time ruler.

| Area | Content | Behavior |
|---|---|---|
| Left rail | Open/import, current recording, channel list, recent analysis sessions | Select a recording or channel; show decode status and source metadata. |
| Center canvas | Overview waveform above a scrollable spectrogram; optional spectrum panel | Shared time axis; zoom/pan; selecting a time range updates spectrum and findings. |
| Right inspector | Selected finding, measurements, evidence components, interpretation limits | Show measured frequency/level/time/channel and harmonic support; source ideas must be labeled as hypotheses. |
| Top bar | Open, analyze, cancel, export, compare, settings | Disable controls only when their action is unavailable; show analysis progress without blocking playback or navigation. |
| Bottom ruler | Time, selection, playhead, visible event markers | Click/drag to seek or select; keyboard navigation must reach each event. |

## Default screen hierarchy

1. File identity and decoded format at the top: filename, rate, channels, bit depth/codec, duration.
2. Waveform gives context for the full recording; spectrogram gives time/frequency structure.
3. Event markers use consistent colors for measured types, with channel identity conveyed by labels/patterns as well as color.
4. Findings panel lists severity by evidence strength and persistence, not by a guessed physical hazard.
5. Selecting a finding highlights its time span, relevant channel, spectral band, and measured harmonics.

## Finding detail

Show the following separately:

- **Measurement:** frequency/range, peak level in dBFS, start/end, duration, channel, drift, and analysis frame/hop.
- **Harmonic measurements:** expected and measured bin frequency, level, local floor, prominence, and supported threshold per partial.
- **Evidence summary:** observed partial count, prominence threshold, temporal stability and missing/weak partials.
- **Interpretation:** “consistent with” candidate labels only. A 50/60 Hz series alone must never be called a confirmed ground loop, appliance, or location.
- **Limitations:** lossy encoding, resampling, filters, short duration, masking, and uncalibrated microphone/electrical path where applicable.

## Interactions

- Opening a file never modifies it. Analysis can be cancelled and restarted.
- Selecting a finding seeks the shared playhead and reveals the full event neighborhood.
- Clicking a channel toggles visibility without mixing channels for detection.
- Zooming changes display detail only; analysis frame settings remain unchanged unless the user explicitly reruns analysis.
- Spectrum cursor reports frequency and dBFS with grid/bin-resolution context.
- A/B comparison aligns timestamps only when the user explicitly requests alignment; automatic alignment must be disclosed.
- Export preview shows report schema/version, settings, and any warnings before writing.

## States and errors

- Empty state explains supported formats and provides Open/drag-and-drop entry points.
- Loading state shows metadata as soon as available and file-read progress for streaming formats.
- Analysis state shows determinate progress where possible, cancellation, and the current channel/frame range.
- Unsupported or malformed files show the specific format or validation failure and a clear next action.
- Empty/silent recordings show “no non-DC energy measured,” never an invented peak.
- Comparison state clearly marks unmatched duration, sample-rate differences, resampling, and alignment choices.

## Accessibility and presentation

- Keyboard access for file opening, channel selection, zoom, seek, finding navigation, and report export.
- Minimum readable text sizes, screen-reader names for controls, visible focus, and high-contrast mode.
- Do not encode severity or channel solely by color.
- Use consistent units (`Hz`, `s`, `dBFS`) and explain terms inline.
- The visual theme should follow the shared Circuit Drift Labs theme specification if available; none was present in the initial checkout.

## Toolkit and implementation boundary

The application shell must call the existing analysis library on a worker and receive immutable progress/results. GUI state must not enter DSP modules. No framework is selected yet: evaluate toolkit licensing, accessibility, Windows/Linux packaging, high-DPI rendering, testing support, and maintenance before adopting JUCE or another UI dependency. The JSON report and analysis core must remain usable if the GUI toolkit changes.

## Acceptance checks

- Window remains responsive while analyzing a long file; cancel works during decode and analysis.
- A finding click moves the playhead and highlights the correct channel/time range.
- Scrolling/zooming does not change analysis results.
- Mono and stereo views never silently downmix.
- Keyboard-only workflow can load, analyze, inspect findings, and export a report.
- No label states a physical cause as verified from audio-only frequency measurements.
