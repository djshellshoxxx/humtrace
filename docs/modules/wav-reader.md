# WAV reader module

## Purpose

Decode common uncompressed RIFF/WAVE PCM and IEEE float samples for offline analysis without altering the input.

## Files and public API

- `include/humtrace/wav.hpp`
- `src/wav.cpp`
- `decode_wav(std::istream&)` returns sample rate, channel count, bit depth, format code, and interleaved normalized samples.
- `WavStreamReader(std::istream&)` scans chunk locations and metadata, then `read_frames(max_frames)` decodes bounded interleaved batches.

## Contract and limits

- C++20 standard library only; input must be seekable and RIFF/WAVE data is read from the stream's current position.
- Supports PCM 8/16/24/32-bit and IEEE float 32/64-bit in little-endian RIFF/WAVE.
- Rejects unsupported format codes, truncated chunks, invalid alignment, incomplete frames, and non-finite float samples.
- `WavStreamReader` retains data-chunk offsets and decodes bounded batches; `decode_wav` is a convenience wrapper that retains all decoded samples.
- No RF64, extensible format, compressed codecs, channel mask, or explicit overall input-file limit.
- Integer PCM maps the representable minimum to -1.0; float samples preserve their numeric value if finite.

## Test and migration

Run `make test`; `tests/test_wav.cpp` builds in-memory WAV fixtures to verify 16/24-bit and float decoding, odd padding, bounded batches, multiple data chunks, metadata, and rejection of non-WAV input. To migrate, copy the two module files, add `src/wav.cpp` to the consumer build, and port the fixture test. Before release, expand fixtures for all advertised formats and fuzz malformed chunk boundaries.

## Reuse candidates

Potentially useful in test tools and small offline audio utilities. Do not treat this reader as a hardened general-purpose decoder until extensible formats, resource limits, fuzzing, and broader malformed-file coverage are implemented.
