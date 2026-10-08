# Integration and build order

## Current dependency graph

```text
WAV reader ──> CLI ──> stdout
Spectrum core ──> CLI
Spectrum core ──> unit tests
WAV reader ──> unit tests
```

The WAV reader and spectrum core have no dependency on each other. The CLI coordinates decoding, channel extraction, framing, analysis, and presentation.

## Integration order

1. Build the spectrum core and tests independently.
2. Build the WAV reader and tests independently.
3. Link both modules into the CLI.
4. Later, add an analysis-result model that stores input metadata, analysis settings, channel identity, timestamps, units, algorithm version, and per-frame measurements.
5. Add the report serializer against that result model.
6. Add the desktop UI as an asynchronous client of the same analysis interface; do not duplicate DSP.
7. Add plugin adapters only after the offline analysis core has stable tests and a clear threading contract.

## Future module handoff

The result model must not depend on GUI widgets. Workers own analysis buffers; UI code receives immutable progress/results through a message queue. File decoding, hashing, and spectral transforms stay off the real-time audio thread. A component copied into another project must include its source, public header, tests, license, build note, and documented units/limitations.
