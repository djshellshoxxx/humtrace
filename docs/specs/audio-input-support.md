# Audio input and decoder specification

## Decoder contract

Define a GUI-independent `AudioReader` interface: metadata query; sequential frame-batch read; optional seek if the format supports it; cancellation-aware reads; precise error/status; and deterministic close. Metadata includes container, codec/subtype, sample rate, channel count/order or unknown-layout flag, container/valid bit depth, total frames/duration when known, and decoder/version.

Returned samples are finite normalized floating-point channel values. Decoder must not downmix or resample implicitly. Integer normalization, float range handling, clipping and malformed sample behavior are documented per codec. Preserve original byte-level hash separately from decoded data.

## Format stages

1. Harden RIFF/WAVE PCM and IEEE float: all currently supported depths, odd chunk padding, unknown chunks, multiple `data` chunks, truncated headers and reads, seek overflow, frame alignment, stream bounds, hostile declared sizes and resource caps.
2. Add `WAVEFORMATEXTENSIBLE`: parse valid bits, subtype GUID, channel mask, and container width. Preserve unknown speaker mapping instead of guessing.
3. Add RF64/BW64 only with 64-bit chunk sizes and fixtures crossing the 32-bit declared-size boundary using sparse/test streams. RF64 derives from RIFF but carries large-file metadata; follow ITU-R BS.2088 / EBU Tech 3306 lineage, not a custom approximation. [1]
4. If broader formats are accepted for v0.1, evaluate one maintained library such as libsndfile for AIFF/FLAC/WAV and a separate codec route for MP3 only if requirements justify it. Pin versions and every enabled codec/license/build option. Existing dependency review recommends libsndfile as leading general audio candidate but defers adoption pending packaging/license review. [2]

## Security and resource controls

Parse untrusted files with checked 64-bit arithmetic, maximum channel/rate/duration and metadata sizes, bounded decoder batches, and fuzzable parser entry points. Never allocate based solely on a file-declared sample count. Decoder errors must include a stage and actionable message without echoing binary payload. Preserve original file; never rewrite metadata.

## Test and portability matrix

Fixture PCM 8/16/24/32, float32/64, extensible valid bits, mono/stereo/multichannel and masks, RF64 size table, multiple/unknown chunks, odd padding, empty data, non-finite float, truncated/oversized sizes, incorrect alignment, seek failure, and mutated input. Run fuzzing with ASan/UBSan. Verify exact samples against independently generated fixtures. Test Windows/Linux native builds and a clean binary package. See existing [WAV module contract](../modules/wav-reader.md).

## Sources

1. ITU-R [Recommendation BS.2088](https://www.itu.int/rec/R-REC-BS.2088/en) and EBU [Tech 3306 RF64 publication page](https://tech.ebu.ch/publications/tech3306); EBU notes Tech 3306 version 2 points to the ITU recommendation.
2. [Open-source component research](../research/existing-open-source.md) and [dependency decisions](../research/research-decisions.md).
