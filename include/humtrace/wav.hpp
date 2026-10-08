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
    std::vector<double> interleaved_samples;
    [[nodiscard]] std::uint64_t frame_count() const noexcept {
        return channels == 0 ? 0 : interleaved_samples.size() / channels;
    }
};

struct WavMetadata {
    std::uint32_t sample_rate_hz{};
    std::uint16_t channels{};
    std::uint16_t bit_depth{};
    std::uint16_t format_code{};
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

// Decode RIFF/WAVE PCM integer (8/16/24/32-bit) and IEEE float (32/64-bit).
// RF64 and WAVEFORMATEXTENSIBLE are not supported yet.
[[nodiscard]] WavAudio decode_wav(std::istream& input);

} // namespace humtrace
