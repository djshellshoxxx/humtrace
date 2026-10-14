# SHA-256 and analysis pipeline modules

## SHA-256 (`sha256.hpp`, `src/sha256.cpp`)

Streaming FIPS 180-4 SHA-256 over bounded 64 KiB chunks; `sha256_hex(istream&)` hashes from the current position to EOF. Verified against NIST vectors (empty, `abc`, 448-bit, one million `a`). The CLI hashes the exact input bytes before decoding and records the digest in the report. This identifies file content; it is not a chain-of-custody claim.

## Pipeline (`pipeline.hpp`, `src/pipeline.cpp`)

`analyze_wav(WavStreamReader&, PipelineSettings, progress)` owns windowing, per-channel measurement, and tracking, returning an `AnalysisReport` without I/O or presentation. `select_window_size` picks the power-of-two window (at most one second or 65536 frames) and enforces the interleaved-window memory bound. The optional progress callback receives a fraction in [0, 1]; an optional `cancelled` callback is polled once per window and aborts with `AnalysisCancelled`. Memory is still O(frames) for results (see [long-file workflow](../specs/long-file-workflow.md)).
