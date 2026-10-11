#pragma once

#include <cstdint>
#include <ios>
#include <istream>
#include <vector>

namespace humtrace {

struct WavAudio {
    std::uint32_t sample_rate_hz{};
    std::uint16_t channels{};
    std::uint16_t bit_depth{};
    std::uint16_t format_code{};
    std::uint16_t valid_bits{};
    std::uint32_t channel_mask{};
    bool extensible{};
    std::vector<double> interleaved_samples;
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
};

class WavStreamReader {
public:
    explicit WavStreamReader(std::istream& input);
    [[nodiscard]] const WavMetadata& metadata() const noexcept { return metadata_; }
    // Returns at most max_frames of interleaved normalized samples.
    [[nodiscard]] std::vector<double> read_frames(std::size_t max_frames);

private:
    struct DataChunk { std::streamoff offset{}; std::uint64_t byte_count{}; };
    std::istream* input_{};
    WavMetadata metadata_;
    std::uint16_t block_align_{};
    std::vector<DataChunk> data_chunks_;
    std::size_t chunk_index_{};
    std::uint64_t chunk_byte_offset_{};
    std::uint64_t frames_read_{};
};

// Decode RIFF/WAVE PCM integer (8/16/24/32-bit) and IEEE float (32/64-bit),
// including WAVE_FORMAT_EXTENSIBLE with PCM or IEEE float subtypes.
// RF64 is not supported yet.
[[nodiscard]] WavAudio decode_wav(std::istream& input);

} // namespace humtrace
