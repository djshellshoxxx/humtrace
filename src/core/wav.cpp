#include "humtrace/wav.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace humtrace {
namespace {
constexpr std::uint16_t wave_format_pcm = 1;
constexpr std::uint16_t wave_format_ieee_float = 3;
constexpr std::uint16_t wave_format_extensible = 0xFFFE;
constexpr std::uint32_t size_placeholder_64 = 0xFFFFFFFFU;
constexpr std::size_t ds64_fixed_bytes = 28;
constexpr std::size_t ds64_entry_bytes = 12;
constexpr std::size_t cancel_poll_chunks = 256;
// KSDATAFORMAT_SUBTYPE_* GUIDs share these bytes after the leading format code.
constexpr std::array<unsigned char, 14> extensible_subtype_suffix{
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

[[noreturn]] void fail(const char* stage, const std::string& message) { throw WavError(stage, message); }

using ChunkId = std::array<char, 4>;

[[nodiscard]] bool id_is(const char* bytes, const char* tag) { return std::memcmp(bytes, tag, 4) == 0; }

[[nodiscard]] std::uint16_t read_u16(const char* bytes) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[0])) |
                                      static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[1]) << 8));
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

// Checked unsigned addition; throws instead of wrapping.
[[nodiscard]] std::uint64_t add_checked(std::uint64_t a, std::uint64_t b, const char* stage) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a)
        fail(stage, "declared size overflows 64-bit file offsets");
    return a + b;
}

// All offsets passed here were bounded by the stream end reported by tellg,
// so they fit in std::streamoff; the check guards against logic errors.
[[nodiscard]] std::streamoff to_streamoff(std::uint64_t value, const char* stage) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        fail(stage, "file offset exceeds the stream offset range");
    return static_cast<std::streamoff>(value);
}

void seek_to(std::istream& input, std::uint64_t offset, const char* stage) {
    input.clear();
    input.seekg(to_streamoff(offset, stage));
    if (!input)
        fail(stage, "failed to seek to byte offset " + std::to_string(offset));
}

void read_exact(std::istream& input, char* destination, std::size_t count, const char* stage, const char* what) {
    if (count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        fail(stage, "read request is too large");
    input.read(destination, static_cast<std::streamsize>(count));
    if (input.gcount() != static_cast<std::streamsize>(count))
        fail(stage, std::string("truncated input while reading ") + what);
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
    if (metadata.format_code == wave_format_pcm)
        return decode_pcm(bytes, metadata.bit_depth);
    if (metadata.bit_depth == 32)
        return static_cast<double>(std::bit_cast<float>(read_u32(bytes)));
    return std::bit_cast<double>(read_u64(bytes));
}

struct Ds64 {
    std::uint64_t riff_size{};
    std::uint64_t data_size{};
    std::uint64_t sample_count{};
    struct Entry { ChunkId id{}; std::uint64_t size{}; bool used{}; };
    std::vector<Entry> table;
    bool data_size_used{};
};

[[nodiscard]] Ds64 parse_ds64(std::istream& input, std::uint64_t chunk_offset, std::uint64_t file_end) {
    seek_to(input, chunk_offset, "ds64");
    std::array<char, 8> header{};
    read_exact(input, header.data(), header.size(), "ds64", "the ds64 chunk header");
    if (!id_is(header.data(), "ds64"))
        fail("ds64", "RF64/BW64 file does not start with a ds64 chunk");
    const std::uint64_t length = read_u32(header.data() + 4);
    if (length < ds64_fixed_bytes)
        fail("ds64", "ds64 chunk is shorter than 28 bytes");
    if (length > wav_limits::max_ds64_chunk_bytes)
        fail("ds64", "ds64 chunk exceeds the " + std::to_string(wav_limits::max_ds64_chunk_bytes) + "-byte limit");
    if (length > file_end - (chunk_offset + 8))
        fail("ds64", "ds64 chunk extends past the end of the stream");
    std::vector<char> body(static_cast<std::size_t>(length));
    read_exact(input, body.data(), body.size(), "ds64", "the ds64 chunk");
    Ds64 ds64;
    ds64.riff_size = read_u64(body.data());
    ds64.data_size = read_u64(body.data() + 8);
    ds64.sample_count = read_u64(body.data() + 16);
    const std::uint32_t table_length = read_u32(body.data() + 24);
    if (table_length > wav_limits::max_ds64_table_entries ||
        ds64_fixed_bytes + ds64_entry_bytes * std::uint64_t{table_length} > length)
        fail("ds64", "ds64 table length does not fit in the ds64 chunk");
    ds64.table.reserve(table_length);
    for (std::uint32_t entry = 0; entry < table_length; ++entry) {
        const char* bytes = body.data() + ds64_fixed_bytes + ds64_entry_bytes * entry;
        Ds64::Entry parsed;
        std::memcpy(parsed.id.data(), bytes, 4);
        parsed.size = read_u64(bytes + 4);
        ds64.table.push_back(parsed);
    }
    return ds64;
}
} // namespace

WavStreamReader::WavStreamReader(std::istream& input, const CancellationToken* cancel)
    : input_(&input), cancel_(cancel) {
    throw_if_cancelled(cancel_);
    metadata_.decoder = std::string(wav_decoder_id);
    const auto start_position = input.tellg();
    if (start_position == std::streampos(-1) || static_cast<std::streamoff>(start_position) < 0)
        fail("header", "input stream must be seekable");
    const auto start = static_cast<std::uint64_t>(static_cast<std::streamoff>(start_position));

    std::array<char, 12> riff_header{};
    input.read(riff_header.data(), static_cast<std::streamsize>(riff_header.size()));
    if (input.gcount() != static_cast<std::streamsize>(riff_header.size()))
        fail("header", "not a RIFF/WAVE file (shorter than the 12-byte header)");
    if (!id_is(riff_header.data() + 8, "WAVE"))
        fail("header", "not a RIFF/WAVE file (missing WAVE form type)");
    const bool is_riff = id_is(riff_header.data(), "RIFF");
    const bool is_rf64 = id_is(riff_header.data(), "RF64");
    const bool is_bw64 = id_is(riff_header.data(), "BW64");
    if (!is_riff && !is_rf64 && !is_bw64)
        fail("header", "not a RIFF/WAVE file (container is not RIFF, RF64 or BW64)");
    metadata_.container = is_riff ? "RIFF" : (is_rf64 ? "RF64" : "BW64");
    const bool large_format = !is_riff;

    input.clear();
    input.seekg(0, std::ios::end);
    const auto file_end_position = input.tellg();
    if (file_end_position == std::streampos(-1))
        fail("header", "failed to determine the stream length");
    const auto file_end = static_cast<std::uint64_t>(static_cast<std::streamoff>(file_end_position));
    if (file_end < start + 12)
        fail("header", "stream ended inside the RIFF header");

    std::optional<Ds64> ds64;
    std::uint64_t riff_size = read_u32(riff_header.data() + 4);
    if (large_format) {
        ds64 = parse_ds64(input, start + 12, file_end);
        if (riff_size == size_placeholder_64)
            riff_size = ds64->riff_size;
        metadata_.declared_sample_count = ds64->sample_count;
    }
    const auto riff_end = add_checked(start + 8, riff_size, "header");
    if (riff_end > file_end)
        fail("header", "declared RIFF size of " + std::to_string(riff_size) +
                           " bytes extends past the end of the stream");
    if (riff_end < start + 12)
        fail("header", "declared RIFF size is smaller than the WAVE header");

    bool have_format = false;
    bool have_data = false;
    std::uint64_t total_data_bytes = 0;
    std::size_t chunk_count = 0;
    std::uint64_t position = start + 12;
    while (riff_end - position >= 8) {
        if (++chunk_count > wav_limits::max_chunks)
            fail("chunks", "more than " + std::to_string(wav_limits::max_chunks) + " chunks");
        if (chunk_count % cancel_poll_chunks == 0)
            throw_if_cancelled(cancel_);
        seek_to(input, position, "chunks");
        std::array<char, 8> chunk_header{};
        read_exact(input, chunk_header.data(), chunk_header.size(), "chunks", "a chunk header");
        const char* id = chunk_header.data();
        const auto length32 = read_u32(chunk_header.data() + 4);
        std::uint64_t length = length32;
        const bool first_chunk = position == start + 12;
        if (large_format && id_is(id, "ds64") && !first_chunk)
            fail("ds64", "more than one ds64 chunk");
        if (large_format && length32 == size_placeholder_64) {
            if (id_is(id, "data") && !ds64->data_size_used) {
                length = ds64->data_size;
                ds64->data_size_used = true;
            } else {
                const auto entry = std::find_if(ds64->table.begin(), ds64->table.end(), [&](const Ds64::Entry& e) {
                    return !e.used && std::memcmp(e.id.data(), id, 4) == 0;
                });
                if (entry == ds64->table.end())
                    fail("ds64", "chunk at byte offset " + std::to_string(position) +
                                     " uses a 64-bit size placeholder without a ds64 entry");
                entry->used = true;
                length = entry->size;
            }
        }
        const auto data_position = position + 8;
        if (length > riff_end - data_position)
            fail("chunks", "chunk at byte offset " + std::to_string(position) + " declares " +
                               std::to_string(length) + " bytes, past the end of the RIFF container");

        if (id_is(id, "fmt ")) {
            if (have_format)
                fail("fmt", "more than one fmt chunk");
            if (length < 16)
                fail("fmt", "fmt chunk is shorter than 16 bytes");
            if (length > wav_limits::max_format_chunk_bytes)
                fail("fmt", "fmt chunk exceeds the " + std::to_string(wav_limits::max_format_chunk_bytes) +
                                "-byte limit");
            std::array<char, 40> format{};
            const auto format_bytes = static_cast<std::size_t>(std::min<std::uint64_t>(length, format.size()));
            read_exact(input, format.data(), format_bytes, "fmt", "the fmt chunk");
            const auto format_tag = read_u16(format.data());
            metadata_.channels = read_u16(format.data() + 2);
            metadata_.sample_rate_hz = read_u32(format.data() + 4);
            block_align_ = read_u16(format.data() + 12);
            metadata_.bit_depth = read_u16(format.data() + 14);
            metadata_.format_code = format_tag;
            metadata_.valid_bits = metadata_.bit_depth;
            if (format_tag == wave_format_extensible) {
                if (format_bytes < 40 || read_u16(format.data() + 16) < 22)
                    fail("fmt", "WAVE_FORMAT_EXTENSIBLE fmt chunk is shorter than 40 bytes");
                metadata_.extensible = true;
                metadata_.valid_bits = read_u16(format.data() + 18);
                metadata_.channel_mask = read_u32(format.data() + 20);
                if (std::memcmp(format.data() + 26, extensible_subtype_suffix.data(),
                                extensible_subtype_suffix.size()) != 0)
                    fail("fmt", "unsupported WAVE_FORMAT_EXTENSIBLE subtype GUID");
                metadata_.format_code = read_u16(format.data() + 24);
            }
            have_format = true;
        } else if (id_is(id, "data")) {
            have_data = true;
            if (length > wav_limits::max_total_data_bytes - total_data_bytes)
                fail("data", "audio data exceeds the " + std::to_string(wav_limits::max_total_data_bytes) +
                                 "-byte limit");
            total_data_bytes += length;
            if (length > 0) {
                if (data_chunks_.size() >= wav_limits::max_data_chunks)
                    fail("data", "more than " + std::to_string(wav_limits::max_data_chunks) + " data chunks");
                data_chunks_.push_back({to_streamoff(data_position, "data"), length});
            }
        }

        // length <= riff_end - data_position, so neither sum can overflow.
        const auto padded = length + (length & 1U);
        if (padded > riff_end - data_position)
            fail("chunks", "odd-sized chunk at byte offset " + std::to_string(position) +
                               " is missing its padding byte");
        position = data_position + padded;
    }
    if (position != riff_end)
        fail("chunks", "RIFF container ends with " + std::to_string(riff_end - position) +
                           " bytes, too few for a chunk header");
    if (!have_format)
        fail("fmt", "missing fmt chunk");
    if (!have_data)
        fail("data", "missing data chunk");
    if (metadata_.channels == 0)
        fail("fmt", "channel count is zero");
    if (metadata_.channels > wav_limits::max_channels)
        fail("fmt", "channel count " + std::to_string(metadata_.channels) + " exceeds the limit of " +
                        std::to_string(wav_limits::max_channels));
    if (metadata_.sample_rate_hz < wav_limits::min_sample_rate_hz ||
        metadata_.sample_rate_hz > wav_limits::max_sample_rate_hz)
        fail("fmt", "sample rate " + std::to_string(metadata_.sample_rate_hz) + " Hz is outside " +
                        std::to_string(wav_limits::min_sample_rate_hz) + ".." +
                        std::to_string(wav_limits::max_sample_rate_hz) + " Hz");
    if (metadata_.format_code != wave_format_pcm && metadata_.format_code != wave_format_ieee_float)
        fail("fmt", "unsupported encoding " + std::to_string(metadata_.format_code) +
                        " (PCM and IEEE float are supported)");
    const bool valid_depth = metadata_.format_code == wave_format_pcm
        ? (metadata_.bit_depth == 8 || metadata_.bit_depth == 16 || metadata_.bit_depth == 24 || metadata_.bit_depth == 32)
        : (metadata_.bit_depth == 32 || metadata_.bit_depth == 64);
    if (!valid_depth)
        fail("fmt", "unsupported bit depth " + std::to_string(metadata_.bit_depth));
    if (std::uint32_t{block_align_} != std::uint32_t{metadata_.channels} * (metadata_.bit_depth / 8U))
        fail("fmt", "block alignment " + std::to_string(block_align_) +
                        " does not equal channels times bytes per sample");
    if (metadata_.extensible) {
        const bool valid_bits_ok = metadata_.format_code == wave_format_pcm
            ? (metadata_.valid_bits >= 1 && metadata_.valid_bits <= metadata_.bit_depth)
            : metadata_.valid_bits == metadata_.bit_depth;
        if (!valid_bits_ok)
            fail("fmt", "WAVE_FORMAT_EXTENSIBLE valid bits are inconsistent with the sample container");
        if (std::popcount(metadata_.channel_mask) > metadata_.channels)
            fail("fmt", "WAVE_FORMAT_EXTENSIBLE channel mask names more speakers than channels");
    }
    for (const auto& chunk : data_chunks_) {
        if (chunk.byte_count % block_align_ != 0)
            fail("data", "data chunk ends with an incomplete sample frame");
    }
    metadata_.frame_count = total_data_bytes / block_align_;
    throw_if_cancelled(cancel_);
}

std::vector<double> WavStreamReader::read_frames(std::size_t max_frames) {
    if (max_frames == 0 || frames_read_ >= metadata_.frame_count)
        return {};
    throw_if_cancelled(cancel_);
    const auto frames_remaining = metadata_.frame_count - frames_read_;
    const auto frame_limit = std::min<std::uint64_t>(static_cast<std::uint64_t>(max_frames), frames_remaining);
    const std::size_t channels = metadata_.channels;
    if (frame_limit > std::numeric_limits<std::size_t>::max() / channels)
        fail("data", "requested batch is too large");
    const auto batch_capacity = static_cast<std::uint64_t>(wav_limits::read_batch_bytes / block_align_) * block_align_;
    const auto bytes_per_sample = static_cast<std::size_t>(metadata_.bit_depth / 8);

    // Grow with data actually read rather than reserving from declared sizes.
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(
        std::min<std::uint64_t>(frame_limit * channels, batch_capacity / bytes_per_sample)));
    std::vector<char> bytes;
    // Commit the read position only after the whole batch succeeded.
    auto chunk_index = chunk_index_;
    auto chunk_byte_offset = chunk_byte_offset_;
    std::uint64_t bytes_remaining = frame_limit * block_align_;
    while (bytes_remaining > 0) {
        while (chunk_index < data_chunks_.size() && chunk_byte_offset == data_chunks_[chunk_index].byte_count) {
            ++chunk_index;
            chunk_byte_offset = 0;
        }
        if (chunk_index >= data_chunks_.size())
            fail("data", "data chunks ended before the declared frame count");
        const auto& chunk = data_chunks_[chunk_index];
        const auto take = std::min({chunk.byte_count - chunk_byte_offset, bytes_remaining, batch_capacity});
        if (take % block_align_ != 0)
            fail("data", "invalid frame alignment while reading");
        bytes.resize(static_cast<std::size_t>(take));
        seek_to(*input_, static_cast<std::uint64_t>(chunk.offset) + chunk_byte_offset, "data");
        read_exact(*input_, bytes.data(), bytes.size(), "data", "audio samples");
        for (std::size_t offset = 0; offset < bytes.size(); offset += bytes_per_sample) {
            const double sample = decode_sample(bytes.data() + offset, metadata_);
            if (!std::isfinite(sample))
                fail("data", "non-finite floating-point sample in frame " +
                                 std::to_string(frames_read_ + (samples.size() / channels)));
            samples.push_back(sample);
        }
        chunk_byte_offset += take;
        bytes_remaining -= take;
        if (bytes_remaining > 0)
            throw_if_cancelled(cancel_);
    }
    chunk_index_ = chunk_index;
    chunk_byte_offset_ = chunk_byte_offset;
    frames_read_ += frame_limit;
    return samples;
}

void WavStreamReader::seek_frame(std::uint64_t frame) {
    if (frame > metadata_.frame_count)
        fail("seek", "frame " + std::to_string(frame) + " is beyond the last frame");
    // frame <= frame_count, so this product is at most the total data size.
    std::uint64_t remaining = frame * block_align_;
    std::size_t index = 0;
    while (index < data_chunks_.size() && remaining >= data_chunks_[index].byte_count) {
        remaining -= data_chunks_[index].byte_count;
        ++index;
    }
    chunk_index_ = index;
    chunk_byte_offset_ = remaining;
    frames_read_ = frame;
}

WavAudio decode_wav(std::istream& input, const CancellationToken* cancel, std::uint64_t max_interleaved_samples) {
    WavStreamReader reader(input, cancel);
    const auto& metadata = reader.metadata();
    WavAudio audio;
    audio.sample_rate_hz = metadata.sample_rate_hz;
    audio.channels = metadata.channels;
    audio.bit_depth = metadata.bit_depth;
    audio.format_code = metadata.format_code;
    audio.valid_bits = metadata.valid_bits;
    audio.channel_mask = metadata.channel_mask;
    audio.extensible = metadata.extensible;
    audio.container = metadata.container;
    audio.decoder = metadata.decoder;
    const auto limit = std::min<std::uint64_t>(max_interleaved_samples, std::numeric_limits<std::size_t>::max());
    if (metadata.frame_count > limit / metadata.channels)
        fail("data", "audio has more than " + std::to_string(limit) + " samples, too many for an in-memory decode");
    // Bounded by the cap above and by the bytes actually present in the stream.
    audio.interleaved_samples.reserve(static_cast<std::size_t>(metadata.frame_count * metadata.channels));
    while (true) {
        const auto batch = reader.read_frames(16384);
        if (batch.empty())
            break;
        audio.interleaved_samples.insert(audio.interleaved_samples.end(), batch.begin(), batch.end());
    }
    return audio;
}

} // namespace humtrace
