# WAV reader module

## Purpose

Decode common uncompressed RIFF/WAVE PCM and IEEE float samples for offline analysis without altering the input.

## Files and public API

- `include/humtrace/wav.hpp`
- `src/wav.cpp`
- `decode_wav(std::istream&)` returns sample rate, channel count, container bit depth, effective format code, valid bits, channel mask, extensible flag, and interleaved normalized samples.
- `WavStreamReader(std::istream&)` scans chunk locations and metadata, then `read_frames(max_frames)` decodes bounded interleaved batches.

## Contract and limits

- C++20 standard library only; input must be seekable and RIFF/WAVE data is read from the stream's current position.
- Supports PCM 8/16/24/32-bit and IEEE float 32/64-bit in little-endian RIFF/WAVE, as legacy format tags 1/3 or `WAVE_FORMAT_EXTENSIBLE` (0xFFFE) with the PCM or IEEE float subtype GUID.
- For extensible headers, `format_code` is the subtype's format code, `bit_depth` is the container width, and `valid_bits` is the declared significant width. Integer samples are normalized by container width, which is correct for the left-justified layout the format requires; padding bits are not inspected. Float requires `valid_bits` equal to the container.
- `channel_mask` is preserved exactly as written. Zero means no declared layout; channels beyond the mask's set bits have no declared speaker position. The reader never infers a layout. A mask naming more speakers than channels is rejected.
- Legacy headers report `valid_bits == bit_depth`, `channel_mask == 0`, and `extensible == false`.
- Rejects unsupported format codes or extensible subtypes, short extensible format chunks (`cbSize` < 22), duplicate `fmt ` chunks, truncated chunks, invalid alignment, incomplete frames, and non-finite float samples.
- `WavStreamReader` retains data-chunk offsets and decodes bounded batches; `decode_wav` is a convenience wrapper that retains all decoded samples.
- No RF64, compressed codecs, or explicit overall input-file limit. Valid bits and channel mask are not yet in the JSON report.
- Integer PCM maps the representable minimum to -1.0; float samples preserve their numeric value if finite.

## Test and migration

Run `make test`; `tests/test_wav.cpp` builds in-memory WAV fixtures to verify 16/24-bit and float decoding, extensible PCM/float headers and their rejection cases, odd padding, bounded batches, multiple data chunks, metadata, and rejection of non-WAV input. To migrate, copy the two module files, add `src/wav.cpp` to the consumer build, and port the fixture test. Before release, expand fixtures for all advertised formats and fuzz malformed chunk boundaries.

## Reuse candidates

Potentially useful in test tools and small offline audio utilities. Do not treat this reader as a hardened general-purpose decoder until resource limits, fuzzing, and broader malformed-file coverage are implemented.
