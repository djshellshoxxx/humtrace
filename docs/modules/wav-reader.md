# WAV reader module

## Purpose

Decode uncompressed RIFF/WAVE, RF64 and BW64 PCM and IEEE float samples for offline analysis without altering the input. The reader treats every file as untrusted.

## Files and public API

- `include/humtrace/wav.hpp`, `src/core/wav.cpp` (C++20 standard library only).
- `WavStreamReader(std::istream&, const CancellationToken* = nullptr)` scans chunk locations and metadata from the stream's current position.
  - `metadata()` returns `WavMetadata`: sample rate, channels, container bit depth, effective format code (1 PCM, 3 float), valid bits, channel mask, `extensible`, `frame_count`, `container` (`"RIFF"`, `"RF64"`, `"BW64"`), `decoder` (`wav_decoder_id`), and `declared_sample_count` (RF64/BW64 ds64 sampleCount as written, informational).
  - `read_frames(max_frames)` returns at most `max_frames` interleaved normalized samples. If it throws, the read position is unchanged.
  - `seek_frame(frame)` and `frame_position()` support random access, including across multiple `data` chunks.
- `decode_wav(std::istream&, const CancellationToken* = nullptr, std::uint64_t max_interleaved_samples = wav_limits::max_in_memory_samples)` decodes everything into `WavAudio` (which also carries `container` and `decoder`). Intended for small files and tests; the cap is caller-visible.
- `wav_decoder_name` / `wav_decoder_version` / `wav_decoder_id` (`"humtrace-wav 1.1"`) identify the decoder in reports.
- `WavError : std::runtime_error` is thrown for every malformed, unsupported, truncated or over-limit input. `stage()` is one of `header`, `ds64`, `chunks`, `fmt`, `data`, `seek`; `what()` is `"WAVE <stage>: <message>"`. Messages contain numbers (offsets, sizes, counts) but never echo bytes from the file.
- Cancellation: the token is polled before scanning, every 256 chunk headers, at the start of each `read_frames` call and between internal read batches. A cancelled operation throws `OperationCancelled` (also a `std::runtime_error`). The token must outlive the reader.

## Supported input

- PCM 8 (unsigned), 16, 24, 32-bit and IEEE float 32/64-bit, little-endian.
- `WAVE_FORMAT_EXTENSIBLE` with the PCM or IEEE float subtype GUID; valid bits and channel mask are reported as written; a zero mask means unspecified layout and is not guessed.
- RF64 (EBU Tech 3306) and BW64 (ITU-R BS.2088): the first chunk must be `ds64`. A 32-bit size of `0xFFFFFFFF` in the container header uses the ds64 RIFF size; the first `data` chunk with `0xFFFFFFFF` uses the ds64 data size; any other chunk with `0xFFFFFFFF` consumes the next unused ds64 table entry with the same ID (rejected if none). Unused table entries are parsed and ignored. Writers that keep real 32-bit sizes are accepted.
- Unknown chunks are skipped; odd-sized chunks must carry their pad byte; multiple `data` chunks are concatenated in file order; bytes after the RIFF container end are ignored.

## Normalization

Integer PCM divides by 2^(container bits - 1), so the representable minimum maps to exactly -1.0 and the maximum to just under +1.0; 8-bit PCM is offset by 128 first. Extensible PCM with fewer valid bits is left-justified, so it is normalized by the container width. Float samples keep their value (no clipping) if finite; NaN and infinities are rejected with a `data`-stage error naming the frame.

## Limits (`humtrace::wav_limits`)

| Constant | Value | Effect |
| --- | --- | --- |
| `max_channels` | 1024 | more channels rejected (`fmt`) |
| `min_sample_rate_hz` / `max_sample_rate_hz` | 1 / 10,000,000 | outside rejected (`fmt`) |
| `max_format_chunk_bytes` | 4096 | larger fmt chunks rejected; at most 40 bytes are read |
| `max_ds64_table_entries` / `max_ds64_chunk_bytes` | 1024 / 12,316 | larger ds64 rejected |
| `max_chunks` | 65,536 | chunk headers walked while scanning |
| `max_data_chunks` | 4096 | non-empty data chunks retained |
| `max_total_data_bytes` | 4 TiB | sum of data chunk sizes |
| `read_batch_bytes` | 1 MiB | bytes read per internal batch (cancellation granularity) |
| `max_in_memory_samples` | 2^28 | default `decode_wav` cap (2 GiB of doubles) |

All offset arithmetic is unsigned 64-bit and checked; every declared size is validated against the RIFF end, which is validated against the stream length reported by seeking to the end, before anything is read or allocated. Read buffers are bounded by `read_batch_bytes`; output vectors grow with data actually decoded, and `decode_wav` reserves only after its cap check.

## Contract notes

- The input must be seekable; the file is opened and read only, never rewritten.
- `frame_count` is derived from data chunk sizes; the ds64 sampleCount is reported but not trusted.
- A truncated file whose RIFF/ds64 size exceeds the stream is rejected rather than partially decoded.
- Not supported: big-endian `RIFX`, compressed codecs (ADPCM, MP3-in-WAV, ...), 64-bit integer PCM, AIFF/FLAC (deferred; see spec stage 4).

## Tests and fuzzing

- `tests/unit/test_wav.cpp`: format coverage, extensible headers, empty data, unknown/odd chunks, every truncation of a valid file, size overruns, alignment, channel/rate caps, non-finite floats, no payload echo, cancellation, seeking.
- `tests/unit/test_wav_rf64.cpp`: RF64/BW64 parsing and malformed ds64; a custom `std::streambuf` presents a sparse virtual stream with a data chunk just over 5 GiB, and the test reads the first frames, frames straddling the 4 GiB offset, and the last frames by seeking, while serving under 1 MiB of bytes.
- `tests/unit/test_wav_mutation.cpp`: about 5,500 deterministic seeded mutations and truncations of PCM8/16/24, float32/64, multi-chunk, extensible and RF64 fixtures; only `std::runtime_error`-derived exceptions may escape.
- `fuzz/fuzz_wav.cpp`: libFuzzer target built with `-DCMAKE_CXX_COMPILER=clang++ -DHUMTRACE_BUILD_FUZZERS=ON` (needs clang's compiler-rt; on Ubuntu `libclang-rt-18-dev`). Seed corpus: `python3 fuzz/make_seeds.py <dir>`, then `build/fuzz/fuzz_wav <dir> -max_total_time=60`. It runs with ASan and UBSan.

## Reuse

Usable as a hardened reader for uncompressed WAV-family files in tools and tests. Other formats need a separate decoder behind the same metadata contract.
