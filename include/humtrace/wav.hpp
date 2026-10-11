#pragma once

#include "humtrace/cancellation.hpp"

#include <cstddef>
#include <cstdint>
#include <ios>
#include <istream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace humtrace {

// Identifies this decoder in reports ("<name> <version>").
inline constexpr std::string_view wav_decoder_name = "humtrace-wav";
inline constexpr std::string_view wav_decoder_version = "1.1";
inline constexpr std::string_view wav_decoder_id = "humtrace-wav 1.1";

// Resource caps applied to untrusted input. Values beyond these are rejected
// before any allocation or long scan takes place.
namespace wav_limits {
inline constexpr std::uint16_t max_channels = 1024;
inline constexpr std::uint32_t min_sample_rate_hz = 1;
inline constexpr std::uint32_t max_sample_rate_hz = 10'000'000;
// fmt chunks larger than this are rejected; at most 40 bytes are ever read.
inline constexpr std::uint64_t max_format_chunk_bytes = 4096;
// ds64 chunks larger than this are rejected (28 fixed bytes + table).
inline constexpr std::uint32_t max_ds64_table_entries = 1024;
inline constexpr std::uint64_t max_ds64_chunk_bytes = 28 + 12ULL * max_ds64_table_entries;
// Chunk headers walked while scanning, and data chunks retained.
inline constexpr std::size_t max_chunks = 65536;
inline constexpr std::size_t max_data_chunks = 4096;
// Sum of all data chunk sizes (4 TiB).
inline constexpr std::uint64_t max_total_data_bytes = 1ULL << 42;
// Bytes read from the stream per internal batch; cancellation is polled
// between batches.
inline constexpr std::size_t read_batch_bytes = 1 << 20;
// Interleaved samples decode_wav() will hold in memory by default (2 GiB of doubles).
inline constexpr std::uint64_t max_in_memory_samples = 1ULL << 28;
} // namespace wav_limits

// Thrown for malformed, unsupported, truncated or over-limit input. stage()
// names the parsing stage ("header", "ds64", "fmt", "chunks", "data", ...).
// Messages never echo bytes from the file.
class WavError : public std::runtime_error {
public:
    WavError(std::string stage, const std::string& message)
        : std::runtime_error("WAVE " + stage + ": " + message), stage_(std::move(stage)) {}
    [[nodiscard]] const std::string& stage() const noexcept { return stage_; }

private:
    std::string stage_;
};

struct WavAudio {
    std::uint32_t sample_rate_hz{};
    std::uint16_t channels{};
    std::uint16_t bit_depth{};
    std::uint16_t format_code{};
    std::uint16_t valid_bits{};
    std::uint32_t channel_mask{};
    bool extensible{};
    std::vector<double> interleaved_samples;
    // "RIFF", "RF64" or "BW64".
    std::string container;
    // Equal to wav_decoder_id.
    std::string decoder;
    [[nodiscard]] std::uint64_t frame_count() const noexcept {
        return channels == 0 ? 0 : interleaved_samples.size() / channels;
    }
};

struct WavMetadata {
    std::uint32_t sample_rate_hz{};
    std::uint16_t channels{};
    // Container width of one sample in bits.
    std::uint16_t bit_depth{};
    // Effective sample encoding: 1 (integer PCM) or 3 (IEEE float). For
    // WAVE_FORMAT_EXTENSIBLE this is taken from the subtype GUID.
    std::uint16_t format_code{};
    // Significant bits per sample; equals bit_depth unless an extensible
    // header declares fewer valid bits.
    std::uint16_t valid_bits{};
    // Speaker-position mask from WAVE_FORMAT_EXTENSIBLE, stored as written.
    // Zero means no mask or an unspecified layout; channels beyond the set
    // bits have no declared position.
    std::uint32_t channel_mask{};
    bool extensible{};
    std::uint64_t frame_count{};
    // "RIFF", "RF64" or "BW64".
    std::string container;
    // Equal to wav_decoder_id.
    std::string decoder;
    // RF64/BW64 ds64 sampleCount as written (informational, not validated);
    // zero for RIFF.
    std::uint64_t declared_sample_count{};
};

class WavStreamReader {
public:
    // Scans chunk locations and metadata from the stream's current position.
    // The stream must be seekable. If cancel is non-null it is polled while
    // scanning and between read batches (throws OperationCancelled). The
    // token must outlive the reader.
    explicit WavStreamReader(std::istream& input, const CancellationToken* cancel = nullptr);
    [[nodiscard]] const WavMetadata& metadata() const noexcept { return metadata_; }
    // Returns at most max_frames of interleaved normalized samples. If the
    // call throws, the read position is unchanged.
    [[nodiscard]] std::vector<double> read_frames(std::size_t max_frames);
    // Moves the read position to frame (0 <= frame <= frame_count).
    void seek_frame(std::uint64_t frame);
    [[nodiscard]] std::uint64_t frame_position() const noexcept { return frames_read_; }

private:
    struct DataChunk { std::streamoff offset{}; std::uint64_t byte_count{}; };
    std::istream* input_{};
    const CancellationToken* cancel_{};
    WavMetadata metadata_;
    std::uint16_t block_align_{};
    std::vector<DataChunk> data_chunks_;
    std::size_t chunk_index_{};
    std::uint64_t chunk_byte_offset_{};
    std::uint64_t frames_read_{};
};

// Decode RIFF/WAVE, RF64 and BW64 PCM integer (8/16/24/32-bit) and IEEE float
// (32/64-bit), including WAVE_FORMAT_EXTENSIBLE with PCM or IEEE float
// subtypes. Retains all samples; rejects files with more than
// max_interleaved_samples samples.
[[nodiscard]] WavAudio decode_wav(std::istream& input, const CancellationToken* cancel = nullptr,
                                  std::uint64_t max_interleaved_samples = wav_limits::max_in_memory_samples);

} // namespace humtrace
