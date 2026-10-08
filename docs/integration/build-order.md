# Integration and build order

## Current dependency graph

```text
WAV reader ──> CLI ──> stdout / JSON report
Spectrum core ──> CLI
Report serializer ──> CLI
Spectrum core ──> unit tests
WAV reader ──> unit tests
```

The WAV reader and spectrum core have no dependency on each other. The CLI coordinates decoding, channel extraction, framing, analysis, and presentation.

## Integration order

1. **Harden input:** complete WAVE parser edge cases and `WAVEFORMATEXTENSIBLE`; fuzz the reader. Add RF64 and broader formats only through the [audio input spec](../specs/audio-input-support.md).
2. **Measurement core:** implement explicit amplitude and PSD definitions, local peak evidence, off-nominal harmonic candidates, and parameter validation using the [analysis engine spec](../specs/analysis-engine.md).
3. **Validation baseline:** create deterministic fixtures and a grouped held-out corpus following [validation requirements](../specs/validation-corpus.md). Keep source attribution outside the measurement engine.
4. **Tracking:** implement per-channel event association and gap/split behavior from [temporal tracking](../specs/temporal-tracking.md).
5. **Bounded job runner:** add cancellation, stage progress, hash integration, and memory limits from [long-file workflow](../specs/long-file-workflow.md).
6. **Reports:** evolve the schema according to [report integrity](../specs/report-integrity.md), write large output incrementally, and test compatibility.
7. **CLI parity:** keep the CLI as a stable integration client and run its end-to-end tests; add progress/cancel flags where useful.
8. **GUI prototype:** build Qt 6 Widgets as a separate CMake target after module/license review. Follow the [GUI spec](../specs/gui.md); do not duplicate DSP or serializer behavior.
9. **Comparison:** integrate [recording comparison](../specs/recording-comparison.md) after event IDs and measurement records stabilize.
10. **Release:** produce clean-machine Windows/Linux packages and complete [packaging gates](../specs/packaging-release.md).
11. Optional plugins or future forensic modules start only after their own scope, validation, and licensing specs are approved.

## Future module handoff

The result model must not depend on GUI widgets. Workers own analysis buffers; UI code receives immutable progress/results through a message queue. File decoding, hashing, and spectral transforms stay off the real-time audio thread. A component copied into another project must include its source, public header, tests, license, build note, and documented units/limitations.
