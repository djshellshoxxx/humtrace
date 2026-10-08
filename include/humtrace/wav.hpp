#pragma once

#include <cstdint>
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

// Decode RIFF/WAVE PCM integer (8/16/24/32-bit) and IEEE float (32/64-bit).
// The v0.1 decoder loads the input stream into memory and does not support RF64.
[[nodiscard]] WavAudio decode_wav(std::istream& input);

} // namespace humtrace
