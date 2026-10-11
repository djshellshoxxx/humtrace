# Audio input and decoder specification

## Decoder contract

Define a GUI-independent `AudioReader` interface: metadata query; sequential frame-batch read; optional seek if the format supports it; cancellation-aware reads; precise error/status; and deterministic close. Metadata includes container, codec/subtype, sample rate, channel count/order or unknown-layout flag, container/valid bit depth, total frames/duration when known, and decoder/version.

Returned samples are finite normalized floating-point channel values. Decoder must not downmix or resample implicitly. Integer normalization, float range handling, clipping and malformed sample behavior are documented per codec. Preserve original byte-level hash separately from decoded data.

## Format stages

1. Harden RIFF/WAVE PCM and IEEE float: all currently supported depths, odd chunk padding, unknown chunks, multiple `data` chunks, truncated headers and reads, seek overflow, frame alignment, stream bounds, hostile declared sizes and resource caps. *Status: implemented in `WavStreamReader` (decoder `humtrace-wav 1.1`): checked 64-bit offsets, named caps in `wav_limits`, stage-tagged `WavError`, cancellation between bounded batches, `seek_frame`, unit tests, a seeded mutation test, and a libFuzzer target run under ASan/UBSan. See [WAV module](../modules/wav-reader.md).*
2. Add `WAVEFORMATEXTENSIBLE`: parse valid bits, subtype GUID, channel mask, and container width. Preserve unknown speaker mapping instead of guessing. *Status: implemented in the reader and CLI for PCM/float subtypes; JSON report fields remain (owned by the report module).*
3. Add RF64/BW64 only with 64-bit chunk sizes and fixtures crossing the 32-bit declared-size boundary using sparse/test streams. RF64 derives from RIFF but carries large-file metadata; follow ITU-R BS.2088 / EBU Tech 3306 lineage, not a custom approximation. [1] *Status: implemented for RF64 and BW64 with a leading ds64 chunk (64-bit RIFF/data sizes, sampleCount exposed as metadata, table entries honoured for other 0xFFFFFFFF-sized chunks); tested with a sparse virtual stream whose data chunk exceeds 4 GiB. `container` and `decoder` are part of reader metadata. Byte-level hashing is provided separately by the [SHA-256 module](../modules/sha256.md).*
4. If broader formats are accepted for v0.1, evaluate one maintained library such as libsndfile for AIFF/FLAC/WAV and a separate codec route for MP3 only if requirements justify it. Pin versions and every enabled codec/license/build option. Existing dependency review recommends libsndfile as leading general audio candidate but defers adoption pending packaging/license review. [2] *Status: deferred; no third-party decoder is added. The `AudioReader` abstraction over multiple codecs is deferred with it; `WavStreamReader` already provides its metadata, batch read, seek, cancellation and error contract.*

## Security and resource controls

Parse untrusted files with checked 64-bit arithmetic, maximum channel/rate/duration and metadata sizes, bounded decoder batches, and fuzzable parser entry points. Never allocate based solely on a file-declared sample count. Decoder errors must include a stage and actionable message without echoing binary payload. Preserve original file; never rewrite metadata.

## Test and portability matrix

Fixture PCM 8/16/24/32, float32/64, extensible valid bits, mono/stereo/multichannel and masks, RF64 size table, multiple/unknown chunks, odd padding, empty data, non-finite float, truncated/oversized sizes, incorrect alignment, seek failure, and mutated input. Run fuzzing with ASan/UBSan. Verify exact samples against independently generated fixtures. Test Windows/Linux native builds and a clean binary package. See existing [WAV module contract](../modules/wav-reader.md).

## Sources

1. ITU-R [Recommendation BS.2088](https://www.itu.int/rec/R-REC-BS.2088/en) and EBU [Tech 3306 RF64 publication page](https://tech.ebu.ch/publications/tech3306); EBU notes Tech 3306 version 2 points to the ITU recommendation.
2. [Open-source component research](../research/existing-open-source.md) and [dependency decisions](../research/research-decisions.md).
