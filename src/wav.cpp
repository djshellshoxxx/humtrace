#include "humtrace/wav.hpp"

#include <bit>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace humtrace {
namespace {
[[nodiscard]] std::uint16_t read_u16(const std::string& data, std::size_t offset) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset])) |
           static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset + 1]) << 8);
}
[[nodiscard]] std::uint32_t read_u32(const std::string& data, std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + byte])) << (byte * 8);
    return value;
}
[[nodiscard]] std::uint64_t read_u64(const std::string& data, std::size_t offset) {
    std::uint64_t value = 0;
    for (unsigned byte = 0; byte < 8; ++byte)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[offset + byte])) << (byte * 8);
    return value;
}
[[nodiscard]] bool tag_is(const std::string& data, std::size_t offset, const char* tag) {
    return data.compare(offset, 4, tag, 4) == 0;
}
[[nodiscard]] double decode_pcm(const std::string& bytes, std::size_t offset, std::uint16_t bits) {
    if (bits == 8)
        return (static_cast<int>(static_cast<unsigned char>(bytes[offset])) - 128) / 128.0;
    std::int64_t raw = 0;
    const auto byte_count = static_cast<std::size_t>(bits / 8);
    for (std::size_t byte = 0; byte < byte_count; ++byte)
        raw |= static_cast<std::int64_t>(static_cast<unsigned char>(bytes[offset + byte])) << (byte * 8);
    const auto sign_bit = std::int64_t{1} << (bits - 1);
    if ((raw & sign_bit) != 0)
        raw -= std::int64_t{1} << bits;
    return static_cast<double>(raw) / static_cast<double>(sign_bit);
}
} // namespace

WavAudio decode_wav(std::istream& input) {
    const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || !tag_is(bytes, 0, "RIFF") || !tag_is(bytes, 8, "WAVE"))
        throw std::runtime_error("not a RIFF/WAVE file");
    const std::uint64_t riff_end64 = static_cast<std::uint64_t>(read_u32(bytes, 4)) + 8;
    if (riff_end64 > bytes.size() || riff_end64 < 12)
        throw std::runtime_error("truncated or invalid RIFF container");
    const auto riff_end = static_cast<std::size_t>(riff_end64);

    bool have_format = false;
    WavAudio audio;
    std::uint16_t block_align = 0;
    std::string sample_bytes;
    for (std::size_t offset = 12; offset + 8 <= riff_end;) {
        const auto length = static_cast<std::size_t>(read_u32(bytes, offset + 4));
        const auto data_offset = offset + 8;
        if (length > riff_end - data_offset)
            throw std::runtime_error("truncated WAVE chunk");
        if (tag_is(bytes, offset, "fmt ")) {
            if (length < 16)
                throw std::runtime_error("invalid WAVE format chunk");
            audio.format_code = read_u16(bytes, data_offset);
            audio.channels = read_u16(bytes, data_offset + 2);
            audio.sample_rate_hz = read_u32(bytes, data_offset + 4);
            block_align = read_u16(bytes, data_offset + 12);
            audio.bit_depth = read_u16(bytes, data_offset + 14);
            have_format = true;
        } else if (tag_is(bytes, offset, "data")) {
            if (length > sample_bytes.max_size() - sample_bytes.size())
                throw std::runtime_error("WAVE data is too large");
            sample_bytes.append(bytes, data_offset, length);
        }
        const auto next = data_offset + length + (length & 1U);
        if (next > riff_end)
            throw std::runtime_error("missing WAVE chunk padding");
        offset = next;
    }
    if (!have_format || sample_bytes.empty())
        throw std::runtime_error("WAVE file is missing format or sample data");
    if (audio.channels == 0 || audio.sample_rate_hz == 0 || block_align == 0)
        throw std::runtime_error("WAVE format has invalid channel, rate, or alignment values");
    if (audio.format_code != 1 && audio.format_code != 3)
        throw std::runtime_error("unsupported WAVE encoding (PCM and IEEE float are supported)");
    const bool valid_depth = audio.format_code == 1
        ? (audio.bit_depth == 8 || audio.bit_depth == 16 || audio.bit_depth == 24 || audio.bit_depth == 32)
        : (audio.bit_depth == 32 || audio.bit_depth == 64);
    if (!valid_depth || block_align != audio.channels * (audio.bit_depth / 8))
        throw std::runtime_error("unsupported or inconsistent WAVE sample format");
    if (sample_bytes.size() % block_align != 0)
        throw std::runtime_error("WAVE data ends with an incomplete sample frame");

    const auto bytes_per_sample = static_cast<std::size_t>(audio.bit_depth / 8);
    const auto sample_count = sample_bytes.size() / bytes_per_sample;
    audio.interleaved_samples.reserve(sample_count);
    for (std::size_t offset = 0; offset < sample_bytes.size(); offset += bytes_per_sample) {
        double sample;
        if (audio.format_code == 1) {
            sample = decode_pcm(sample_bytes, offset, audio.bit_depth);
        } else if (audio.bit_depth == 32) {
            const auto raw = static_cast<std::uint32_t>(read_u32(sample_bytes, offset));
            sample = static_cast<double>(std::bit_cast<float>(raw));
        } else {
            sample = std::bit_cast<double>(read_u64(sample_bytes, offset));
        }
        if (!std::isfinite(sample))
            throw std::runtime_error("WAVE contains a non-finite floating-point sample");
        audio.interleaved_samples.push_back(sample);
    }
    return audio;
}

} // namespace humtrace
