// RF64 / BW64 (EBU Tech 3306, ITU-R BS.2088) container tests, including a
// sparse virtual stream whose data chunk is larger than 4 GiB.
#include "humtrace/wav.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void u16(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value & 0xff));
    bytes.push_back(static_cast<char>((value >> 8) & 0xff));
}
void u32(std::string& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}
void u64(std::string& bytes, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
        bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}
constexpr std::uint32_t placeholder = 0xFFFFFFFFU;

struct TableEntry { std::string id; std::uint64_t size; };
struct Rf64Spec {
    std::string container{"RF64"};
    std::uint32_t riff32{placeholder};
    std::uint64_t riff64{};          // 0: computed from the file size
    std::uint64_t data64{};          // ds64 data size
    std::uint64_t sample_count{};
    std::vector<TableEntry> table;
    std::uint32_t table_length_override{0};
    bool override_table_length{false};
    std::uint16_t format{1}, channels{2}, bits{16};
    std::uint32_t data32{placeholder};
    std::string pre_data_chunks;     // raw chunk bytes between fmt and data
};

std::string ds64_chunk(const Rf64Spec& spec, std::uint64_t riff64) {
    std::string body;
    u64(body, riff64); u64(body, spec.data64); u64(body, spec.sample_count);
    u32(body, spec.override_table_length ? spec.table_length_override
                                         : static_cast<std::uint32_t>(spec.table.size()));
    for (const auto& entry : spec.table) { body += entry.id; u64(body, entry.size); }
    std::string chunk = "ds64";
    u32(chunk, static_cast<std::uint32_t>(body.size()));
    return chunk + body;
}

// Header bytes up to and including the data chunk header. total_size is the
// size of the whole (possibly virtual) file.
std::string rf64_header(const Rf64Spec& spec, std::uint64_t total_size) {
    std::string fmt = "fmt ";
    u32(fmt, 16);
    const auto align = static_cast<std::uint16_t>(spec.channels * (spec.bits / 8));
    u16(fmt, spec.format); u16(fmt, spec.channels); u32(fmt, 48000); u32(fmt, 48000U * align);
    u16(fmt, align); u16(fmt, spec.bits);
    std::string data_header = "data";
    u32(data_header, spec.data32);
    const auto riff64 = spec.riff64 != 0 ? spec.riff64 : total_size - 8;
    std::string result = spec.container;
    u32(result, spec.riff32);
    result += "WAVE";
    result += ds64_chunk(spec, riff64) + fmt + spec.pre_data_chunks + data_header;
    return result;
}

std::string small_rf64(Rf64Spec spec, const std::string& data) {
    spec.data64 = data.size();
    // Header size does not depend on the sizes written into it.
    const auto header_size = rf64_header(spec, 1000).size();
    return rf64_header(spec, header_size + data.size()) + data;
}

std::string error_of(const std::string& bytes) {
    std::istringstream stream(bytes, std::ios::binary);
    try { (void)humtrace::decode_wav(stream); }
    catch (const std::runtime_error& error) { return error.what(); }
    return {};
}

// A read-only seekable stream of `size` bytes: `header` followed by a
// deterministic sample pattern generated on demand. Nothing near the full
// size is ever allocated.
class SparseWavBuffer : public std::streambuf {
public:
    SparseWavBuffer(std::string header, std::uint64_t size) : header_(std::move(header)), size_(size) {
        setg(buffer_.data(), buffer_.data(), buffer_.data());
    }
    [[nodiscard]] std::uint64_t bytes_served() const noexcept { return served_; }
    [[nodiscard]] std::uint64_t data_start() const noexcept { return header_.size(); }
    [[nodiscard]] static std::uint16_t sample_value(std::uint64_t frame, unsigned channel) {
        return static_cast<std::uint16_t>((frame * 3 + channel * 7 + (frame >> 20)) & 0xffff);
    }

protected:
    int_type underflow() override {
        const auto current = base_ + static_cast<std::uint64_t>(gptr() - eback());
        if (current >= size_)
            return traits_type::eof();
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(buffer_.size(), size_ - current));
        for (std::size_t i = 0; i < count; ++i)
            buffer_[i] = byte_at(current + i);
        base_ = current;
        served_ += count;
        setg(buffer_.data(), buffer_.data(), buffer_.data() + count);
        return traits_type::to_int_type(buffer_[0]);
    }
    pos_type seekoff(off_type offset, std::ios_base::seekdir direction, std::ios_base::openmode) override {
        const auto current = static_cast<off_type>(base_) + (gptr() - eback());
        off_type origin = direction == std::ios_base::beg ? 0
                        : direction == std::ios_base::cur ? current
                                                          : static_cast<off_type>(size_);
        const auto target = origin + offset;
        if (target < 0 || static_cast<std::uint64_t>(target) > size_)
            return pos_type(off_type(-1));
        base_ = static_cast<std::uint64_t>(target);
        setg(buffer_.data(), buffer_.data(), buffer_.data());
        return pos_type(target);
    }
    pos_type seekpos(pos_type position, std::ios_base::openmode mode) override {
        return seekoff(off_type(position), std::ios_base::beg, mode);
    }

private:
    [[nodiscard]] char byte_at(std::uint64_t position) const {
        if (position < header_.size())
            return header_[static_cast<std::size_t>(position)];
        const auto offset = position - header_.size();
        const auto value = sample_value(offset / 4, static_cast<unsigned>((offset % 4) / 2));
        return static_cast<char>((offset % 2) == 0 ? (value & 0xff) : (value >> 8));
    }
    std::string header_;
    std::uint64_t size_;
    std::uint64_t base_{};
    std::uint64_t served_{};
    std::array<char, 4096> buffer_{};
};

double expected_sample(std::uint64_t frame, unsigned channel) {
    const auto raw = static_cast<std::int16_t>(SparseWavBuffer::sample_value(frame, channel));
    return static_cast<double>(raw) / 32768.0;
}

void reads_first_and_last_frames_of_a_sparse_stream_above_4_gib() {
    const std::uint64_t frames = (5ULL << 30) / 4 + 3;  // stereo PCM16, just over 5 GiB of data
    const std::uint64_t data_bytes = frames * 4;
    Rf64Spec spec;
    spec.data64 = data_bytes;
    spec.sample_count = frames;
    // A LIST chunk whose size comes from the ds64 table, plus an unused table entry.
    spec.table = {{"LIST", 6}, {"junk", 0x123456789ULL}};
    spec.pre_data_chunks = "LIST";
    u32(spec.pre_data_chunks, placeholder);
    spec.pre_data_chunks += std::string("abcdef", 6);
    const auto header_size = rf64_header(spec, 1000).size();
    const auto total = header_size + data_bytes;
    SparseWavBuffer buffer(rf64_header(spec, total), total);
    std::istream stream(&buffer);

    humtrace::WavStreamReader reader(stream);
    const auto& metadata = reader.metadata();
    require(metadata.container == "RF64", "RF64 container is reported");
    require(metadata.frame_count == frames && frames > (1ULL << 32) / 4, "64-bit data size gives the frame count");
    require(metadata.declared_sample_count == frames, "ds64 sample count is exposed");
    require(metadata.channels == 2 && metadata.bit_depth == 16 && metadata.decoder == "humtrace-wav 1.1",
            "RF64 format metadata");

    const auto first = reader.read_frames(3);
    require(first.size() == 6, "first frames are read");
    for (unsigned i = 0; i < 6; ++i)
        require(first[i] == expected_sample(i / 2, i % 2), "first frame samples match the pattern");

    reader.seek_frame(frames - 2);
    const auto last = reader.read_frames(10);
    require(last.size() == 4 && reader.frame_position() == frames, "only the last frames remain");
    for (unsigned i = 0; i < 4; ++i)
        require(last[i] == expected_sample(frames - 2 + i / 2, i % 2), "last frame samples match the pattern");

    // Frames straddling the 4 GiB data offset.
    const std::uint64_t boundary = (1ULL << 32) / 4;
    reader.seek_frame(boundary - 1);
    const auto around = reader.read_frames(2);
    require(around.size() == 4 && around[0] == expected_sample(boundary - 1, 0) &&
                around[3] == expected_sample(boundary, 1),
            "frames across the 32-bit boundary are addressed with 64-bit offsets");
    require(reader.read_frames(0).empty(), "zero-frame read is empty");
    require(buffer.bytes_served() < (1U << 20), "the virtual stream was not read in full");
}

void sparse_stream_rejects_data_beyond_the_stream() {
    Rf64Spec spec;
    spec.data64 = 6ULL << 30;
    const auto header_size = rf64_header(spec, 1000).size();
    const auto total = header_size + (5ULL << 30);  // stream is 1 GiB shorter than declared
    spec.riff64 = total - 8;
    SparseWavBuffer buffer(rf64_header(spec, total), total);
    std::istream stream(&buffer);
    bool threw = false;
    try { humtrace::WavStreamReader reader(stream); } catch (const humtrace::WavError& error) {
        threw = error.stage() == "chunks";
    }
    require(threw, "ds64 data size beyond the RIFF end is rejected without allocation");
}

void decodes_small_rf64_and_bw64() {
    std::string data;
    u16(data, 0x4000); u16(data, 0x8000);
    Rf64Spec spec;
    std::istringstream rf64(small_rf64(spec, data), std::ios::binary);
    const auto audio = humtrace::decode_wav(rf64);
    require(audio.container == "RF64" && audio.interleaved_samples.size() == 2 &&
                audio.interleaved_samples[0] == 0.5 && audio.interleaved_samples[1] == -1.0,
            "small RF64 decodes");

    Rf64Spec bw64;
    bw64.container = "BW64"; bw64.format = 3; bw64.channels = 1; bw64.bits = 32;
    std::istringstream bw(small_rf64(bw64, std::string("\0\0\x80\x3e", 4)), std::ios::binary);
    const auto bw_audio = humtrace::decode_wav(bw);
    require(bw_audio.container == "BW64" && bw_audio.interleaved_samples.size() == 1 &&
                bw_audio.interleaved_samples[0] == 0.25,
            "BW64 float decodes");

    // A writer may keep real 32-bit sizes when they fit.
    Rf64Spec real_sizes;
    real_sizes.data32 = static_cast<std::uint32_t>(data.size());
    real_sizes.data64 = 999;  // ignored because the data size is not a placeholder
    auto bytes = rf64_header(real_sizes, 1000);
    real_sizes.riff32 = static_cast<std::uint32_t>(bytes.size() + data.size() - 8);
    bytes = rf64_header(real_sizes, bytes.size() + data.size()) + data;
    std::istringstream real(bytes, std::ios::binary);
    require(humtrace::decode_wav(real).interleaved_samples.size() == 2, "RF64 with 32-bit sizes decodes");
}

void rejects_malformed_ds64() {
    std::string data(4, '\0');
    auto missing = small_rf64({}, data);
    missing.replace(12, 4, "JUNK");
    require(error_of(missing).find("ds64") != std::string::npos, "RF64 without a leading ds64 is rejected");

    Rf64Spec overrun;
    overrun.override_table_length = true;
    overrun.table_length_override = 5;
    require(error_of(small_rf64(overrun, data)).find("table") != std::string::npos,
            "ds64 table longer than its chunk is rejected");

    Rf64Spec unmatched;
    unmatched.pre_data_chunks = "LIST";
    u32(unmatched.pre_data_chunks, placeholder);
    require(error_of(small_rf64(unmatched, data)).find("placeholder") != std::string::npos,
            "placeholder size without a table entry is rejected");

    Rf64Spec huge_riff;
    huge_riff.riff64 = 1ULL << 40;
    require(error_of(small_rf64(huge_riff, data)).find("header") != std::string::npos,
            "ds64 RIFF size beyond the stream is rejected");

    Rf64Spec overflow;
    overflow.riff64 = ~0ULL;
    require(!error_of(small_rf64(overflow, data)).empty(), "ds64 RIFF size near 2^64 is rejected");

    Rf64Spec huge_data;
    auto bytes = small_rf64(huge_data, data);
    bytes[20 + 8 + 7] = '\x7f';  // ds64 dataSize high byte
    require(!error_of(bytes).empty(), "ds64 data size near 2^63 is rejected");

    Rf64Spec second;
    second.pre_data_chunks = ds64_chunk(second, 0);
    require(error_of(small_rf64(second, data)).find("more than one ds64") != std::string::npos,
            "a second ds64 chunk is rejected");

    auto short_ds64 = small_rf64({}, data);
    short_ds64[16] = 20;
    require(!error_of(short_ds64).empty(), "ds64 shorter than 28 bytes is rejected");

    Rf64Spec misaligned;
    auto odd = data + "\x01\x02";
    odd.resize(6);
    require(!error_of(small_rf64(misaligned, odd)).empty(), "ds64 data size with a partial frame is rejected");
}
} // namespace

int main() {
    reads_first_and_last_frames_of_a_sparse_stream_above_4_gib();
    sparse_stream_rejects_data_beyond_the_stream();
    decodes_small_rf64_and_bw64();
    rejects_malformed_ds64();
    std::cout << "All RF64/BW64 tests passed.\n";
}
