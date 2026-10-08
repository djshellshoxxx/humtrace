#include "humtrace/wav.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace humtrace {
namespace {
void read_exact(std::istream& input, char* destination, std::size_t count) {
    if (count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("WAVE read request is too large");
    input.read(destination, static_cast<std::streamsize>(count));
    if (input.gcount() != static_cast<std::streamsize>(count))
        throw std::runtime_error("truncated WAVE file");
}

[[nodiscard]] std::uint16_t read_u16(const char* bytes) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[0])) |
           static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[1]) << 8);
}

[[nodiscard]] std::uint32_t read_u32(const char* bytes) {
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[byte])) << (byte * 8);
    return value;
}

[[nodiscard]] std::uint64_t read_u64(const char* bytes) {
    std::uint64_t value = 0;
    for (unsigned byte = 0; byte < 8; ++byte)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[byte])) << (byte * 8);
    return value;
}

[[nodiscard]] double decode_pcm(const char* bytes, std::uint16_t bits) {
    if (bits == 8)
        return (static_cast<int>(static_cast<unsigned char>(bytes[0])) - 128) / 128.0;
    std::int64_t raw = 0;
    const auto byte_count = static_cast<std::size_t>(bits / 8);
    for (std::size_t byte = 0; byte < byte_count; ++byte)
        raw |= static_cast<std::int64_t>(static_cast<unsigned char>(bytes[byte])) << (byte * 8);
    const auto sign_bit = std::int64_t{1} << (bits - 1);
    if ((raw & sign_bit) != 0)
        raw -= std::int64_t{1} << bits;
    return static_cast<double>(raw) / static_cast<double>(sign_bit);
}

[[nodiscard]] double decode_sample(const char* bytes, const WavMetadata& metadata) {
    double sample;
    if (metadata.format_code == 1) {
        sample = decode_pcm(bytes, metadata.bit_depth);
    } else if (metadata.bit_depth == 32) {
        sample = static_cast<double>(std::bit_cast<float>(read_u32(bytes)));
    } else {
        sample = std::bit_cast<double>(read_u64(bytes));
    }
    if (!std::isfinite(sample))
        throw std::runtime_error("WAVE contains a non-finite floating-point sample");
    return sample;
}

[[nodiscard]] bool tag_is(const std::array<char, 8>& header, const char* tag) {
    return std::memcmp(header.data(), tag, 4) == 0;
}
} // namespace

WavStreamReader::WavStreamReader(std::istream& input) : input_(&input) {
    const auto start_position = input.tellg();
    if (start_position == std::streampos(-1))
        throw std::runtime_error("WAVE input must be seekable");
    const auto start = static_cast<std::streamoff>(start_position);

    std::array<char, 12> riff_header{};
    read_exact(input, riff_header.data(), riff_header.size());
    if (std::memcmp(riff_header.data(), "RIFF", 4) != 0 ||
        std::memcmp(riff_header.data() + 8, "WAVE", 4) != 0)
        throw std::runtime_error("not a RIFF/WAVE file");
    const auto riff_length = read_u32(riff_header.data() + 4);
    const auto riff_end = start + 8 + static_cast<std::streamoff>(riff_length);

    input.clear();
    input.seekg(0, std::ios::end);
    const auto file_end_position = input.tellg();
    if (file_end_position == std::streampos(-1) || riff_end > static_cast<std::streamoff>(file_end_position) ||
        riff_end < start + 12)
        throw std::runtime_error("truncated or invalid RIFF container");

    bool have_format = false;
    std::uint64_t total_data_bytes = 0;
    std::streamoff position = start + 12;
    while (position + 8 <= riff_end) {
        input.clear();
        input.seekg(position);
        if (!input)
            throw std::runtime_error("failed to seek to WAVE chunk");
        std::array<char, 8> chunk_header{};
        read_exact(input, chunk_header.data(), chunk_header.size());
        const auto length = static_cast<std::uint64_t>(read_u32(chunk_header.data() + 4));
        const auto data_position = position + 8;
        if (length > static_cast<std::uint64_t>(riff_end - data_position))
            throw std::runtime_error("truncated WAVE chunk");

        if (tag_is(chunk_header, "fmt ")) {
            if (length < 16)
                throw std::runtime_error("invalid WAVE format chunk");
            std::array<char, 16> format{};
            read_exact(input, format.data(), format.size());
            metadata_.format_code = read_u16(format.data());
            metadata_.channels = read_u16(format.data() + 2);
            metadata_.sample_rate_hz = read_u32(format.data() + 4);
            block_align_ = read_u16(format.data() + 12);
            metadata_.bit_depth = read_u16(format.data() + 14);
            have_format = true;
        } else if (tag_is(chunk_header, "data")) {
            if (length > std::numeric_limits<std::uint64_t>::max() - total_data_bytes)
                throw std::runtime_error("WAVE sample data is too large");
            data_chunks_.push_back({data_position, length});
            total_data_bytes += length;
        }

        const auto next_position = data_position + static_cast<std::streamoff>(length + (length & 1U));
        if (next_position > riff_end)
            throw std::runtime_error("missing WAVE chunk padding");
        position = next_position;
    }
    if (position != riff_end)
        throw std::runtime_error("truncated WAVE chunk header");
    if (!have_format || data_chunks_.empty())
        throw std::runtime_error("WAVE file is missing format or sample data");
    if (metadata_.channels == 0 || metadata_.sample_rate_hz == 0 || block_align_ == 0)
        throw std::runtime_error("WAVE format has invalid channel, rate, or alignment values");
    if (metadata_.format_code != 1 && metadata_.format_code != 3)
        throw std::runtime_error("unsupported WAVE encoding (PCM and IEEE float are supported)");
    const bool valid_depth = metadata_.format_code == 1
        ? (metadata_.bit_depth == 8 || metadata_.bit_depth == 16 || metadata_.bit_depth == 24 || metadata_.bit_depth == 32)
        : (metadata_.bit_depth == 32 || metadata_.bit_depth == 64);
    if (!valid_depth || block_align_ != metadata_.channels * (metadata_.bit_depth / 8))
        throw std::runtime_error("unsupported or inconsistent WAVE sample format");
    for (const auto& chunk : data_chunks_) {
        if (chunk.byte_count % block_align_ != 0)
            throw std::runtime_error("WAVE data chunk ends with an incomplete sample frame");
    }
    if (total_data_bytes % block_align_ != 0)
        throw std::runtime_error("WAVE data ends with an incomplete sample frame");
    metadata_.frame_count = total_data_bytes / block_align_;
}

std::vector<double> WavStreamReader::read_frames(std::size_t max_frames) {
    if (max_frames == 0 || frames_read_ >= metadata_.frame_count)
        return {};
    const auto frames_remaining = metadata_.frame_count - frames_read_;
    const auto frame_limit = std::min<std::uint64_t>(static_cast<std::uint64_t>(max_frames), frames_remaining);
    const auto byte_count = frame_limit * block_align_;
    if (frame_limit > std::numeric_limits<std::size_t>::max() / metadata_.channels ||
        byte_count > std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("requested WAVE batch is too large");
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(frame_limit) * metadata_.channels);
    std::uint64_t bytes_remaining = byte_count;
    const auto bytes_per_sample = static_cast<std::size_t>(metadata_.bit_depth / 8);
    while (bytes_remaining > 0) {
        while (chunk_index_ < data_chunks_.size() && chunk_byte_offset_ == data_chunks_[chunk_index_].byte_count) {
            ++chunk_index_;
            chunk_byte_offset_ = 0;
        }
        if (chunk_index_ >= data_chunks_.size())
            throw std::runtime_error("WAVE data chunks ended before declared frame count");
        const auto& chunk = data_chunks_[chunk_index_];
        const auto available = chunk.byte_count - chunk_byte_offset_;
        const auto take = std::min(available, bytes_remaining);
        if (take % block_align_ != 0 || take > std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("invalid WAVE frame alignment while reading");
        std::vector<char> bytes(static_cast<std::size_t>(take));
        input_->clear();
        input_->seekg(chunk.offset + static_cast<std::streamoff>(chunk_byte_offset_));
        if (!*input_)
            throw std::runtime_error("failed to seek to WAVE audio samples");
        read_exact(*input_, bytes.data(), bytes.size());
        for (std::size_t offset = 0; offset < bytes.size(); offset += bytes_per_sample)
            samples.push_back(decode_sample(bytes.data() + offset, metadata_));
        chunk_byte_offset_ += take;
        bytes_remaining -= take;
    }
    frames_read_ += frame_limit;
    return samples;
}

WavAudio decode_wav(std::istream& input) {
    WavStreamReader reader(input);
    const auto& metadata = reader.metadata();
    WavAudio audio{metadata.sample_rate_hz, metadata.channels, metadata.bit_depth,
                   metadata.format_code, {}};
    if (metadata.frame_count > std::numeric_limits<std::size_t>::max() / metadata.channels)
        throw std::runtime_error("WAVE file is too large for an in-memory decode");
    audio.interleaved_samples.reserve(static_cast<std::size_t>(metadata.frame_count) * metadata.channels);
    while (true) {
        auto batch = reader.read_frames(16384);
        if (batch.empty())
            break;
        audio.interleaved_samples.insert(audio.interleaved_samples.end(), batch.begin(), batch.end());
    }
    return audio;
}

} // namespace humtrace
