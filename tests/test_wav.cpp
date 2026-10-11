#include "humtrace/wav.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void u16(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value & 0xff));
    bytes.push_back(static_cast<char>((value >> 8) & 0xff));
}
void u32(std::string& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}
void tag(std::string& bytes, const char* value) { bytes.append(value, 4); }
std::string pcm16_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
    u32(body, 8000); u32(body, 16000); u16(body, 2); u16(body, 16);
    tag(body, "data"); u32(body, 4); u16(body, 16384); u16(body, 0x8000);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string pcm24_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
    u32(body, 48000); u32(body, 144000); u16(body, 3); u16(body, 24);
    tag(body, "data"); u32(body, 3); body.append("\0\0\x80", 3); body.push_back('\0');
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string float32_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 3); u16(body, 1);
    u32(body, 48000); u32(body, 192000); u16(body, 4); u16(body, 32);
    tag(body, "data"); u32(body, 4); body.append("\0\0\x80\x3e", 4);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string pcm16_multiple_data_chunks_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
    u32(body, 8000); u32(body, 16000); u16(body, 2); u16(body, 16);
    tag(body, "data"); u32(body, 2); u16(body, 16384);
    tag(body, "data"); u32(body, 2); u16(body, 0x8000);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
struct ExtensibleFormat {
    std::uint16_t channels{2};
    std::uint16_t container_bits{24};
    std::uint16_t valid_bits{20};
    std::uint32_t channel_mask{0x3};
    std::uint16_t subtype{1};
    std::uint16_t cb_size{22};
    std::uint32_t fmt_length{40};
};
std::string extensible_wav(const ExtensibleFormat& format, const std::string& data) {
    const auto block_align = static_cast<std::uint16_t>(format.channels * (format.container_bits / 8));
    std::string body;
    tag(body, "fmt "); u32(body, format.fmt_length); u16(body, 0xFFFE); u16(body, format.channels);
    u32(body, 48000); u32(body, 48000U * block_align); u16(body, block_align); u16(body, format.container_bits);
    u16(body, format.cb_size); u16(body, format.valid_bits); u32(body, format.channel_mask);
    u16(body, format.subtype); u16(body, 0); u16(body, 0); u16(body, 0x10);
    body.append("\x80\x00\x00\xAA\x00\x38\x9B\x71", 8);
    body.resize(8 + format.fmt_length, '\0');
    tag(body, "data"); u32(body, static_cast<std::uint32_t>(data.size())); body += data;
    if (data.size() % 2 != 0) body.push_back('\0');
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string pcm16_two_format_chunks_wav() {
    std::string body;
    for (int copy = 0; copy < 2; ++copy) {
        tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
        u32(body, 8000); u32(body, 16000); u16(body, 2); u16(body, 16);
    }
    tag(body, "data"); u32(body, 2); u16(body, 0);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
bool decode_throws(const std::string& bytes) {
    std::istringstream stream(bytes, std::ios::binary);
    try { (void)humtrace::decode_wav(stream); }
    catch (const std::runtime_error&) { return true; }
    return false;
}
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void decodes_pcm16_metadata_and_samples() {
    std::istringstream stream(pcm16_wav(), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.sample_rate_hz == 8000, "sample rate is preserved");
    require(audio.channels == 1, "channel count is preserved");
    require(audio.bit_depth == 16, "bit depth is preserved");
    require(audio.interleaved_samples.size() == 2, "sample frames are decoded");
    require(audio.interleaved_samples[0] > 0.49 && audio.interleaved_samples[0] < 0.51,
            "positive PCM sample is normalized");
    require(audio.interleaved_samples[1] == -1.0, "minimum PCM sample is normalized");
}
void rejects_non_wave_input() {
    std::istringstream stream("NOPE", std::ios::binary);
    bool threw = false;
    try { (void)humtrace::decode_wav(stream); }
    catch (const std::runtime_error&) { threw = true; }
    require(threw, "invalid container is rejected");
}
void decodes_signed_24_bit_minimum() {
    std::istringstream stream(pcm24_wav(), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.bit_depth == 24, "24-bit depth is preserved");
    require(audio.interleaved_samples.size() == 1, "odd-sized data chunk padding is skipped");
    require(audio.interleaved_samples[0] == -1.0, "24-bit sign extension is correct");
}
void decodes_ieee_float32_samples() {
    std::istringstream stream(float32_wav(), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.format_code == 3, "IEEE float format code is preserved");
    require(audio.bit_depth == 32, "float bit depth is preserved");
    require(audio.interleaved_samples.size() == 1, "one float sample is decoded");
    require(std::abs(audio.interleaved_samples[0] - 0.25) < 1e-7,
            "little-endian IEEE float sample is decoded");
}
void streaming_reader_returns_bounded_frame_batches() {
    std::istringstream stream(pcm16_wav(), std::ios::binary);
    humtrace::WavStreamReader reader(stream);
    require(reader.metadata().frame_count == 2, "stream reader exposes total frame count");
    const auto first = reader.read_frames(1);
    const auto second = reader.read_frames(1);
    const auto end = reader.read_frames(8);
    require(first.size() == 1 && second.size() == 1, "reader respects requested frame batch size");
    require(first[0] > 0.49 && first[0] < 0.51 && second[0] == -1.0,
            "streaming sample order and normalization are preserved");
    require(end.empty(), "reader returns an empty batch at end of audio");
}
void streaming_reader_continues_across_data_chunks() {
    std::istringstream stream(pcm16_multiple_data_chunks_wav(), std::ios::binary);
    humtrace::WavStreamReader reader(stream);
    const auto samples = reader.read_frames(8);
    require(samples.size() == 2, "frames across multiple data chunks are returned together");
    require(samples[0] > 0.49 && samples[0] < 0.51 && samples[1] == -1.0,
            "sample order is retained across data chunk boundaries");
}
void decodes_extensible_pcm_with_valid_bits_and_mask() {
    // 20 valid bits left-justified in a 24-bit container: left = +0.5, right = minimum.
    const std::string data("\x00\x00\x40\x00\x00\x80", 6);
    std::istringstream stream(extensible_wav({}, data), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.extensible, "extensible header is reported");
    require(audio.format_code == 1, "PCM subtype GUID maps to integer PCM");
    require(audio.bit_depth == 24 && audio.valid_bits == 20, "container width and valid bits are kept apart");
    require(audio.channel_mask == 0x3, "channel mask is preserved as written");
    require(audio.interleaved_samples.size() == 2 && audio.interleaved_samples[0] == 0.5 &&
                audio.interleaved_samples[1] == -1.0,
            "left-justified extensible PCM normalizes by container width");
}
void decodes_extensible_float_with_unspecified_layout() {
    ExtensibleFormat format;
    format.channels = 3; format.container_bits = 32; format.valid_bits = 32;
    format.channel_mask = 0; format.subtype = 3;
    const std::string data("\0\0\x80\x3e\0\0\x80\xbe\0\0\0\0", 12);
    std::istringstream stream(extensible_wav(format, data), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.format_code == 3 && audio.channels == 3, "float subtype GUID maps to IEEE float");
    require(audio.channel_mask == 0, "an unspecified speaker layout is not guessed");
    require(audio.interleaved_samples.size() == 3 && audio.interleaved_samples[0] == 0.25 &&
                audio.interleaved_samples[1] == -0.25 && audio.interleaved_samples[2] == 0.0,
            "extensible float samples are decoded");
}
void legacy_formats_report_full_valid_bits_and_no_mask() {
    std::istringstream stream(pcm16_wav(), std::ios::binary);
    const humtrace::WavStreamReader reader(stream);
    require(!reader.metadata().extensible && reader.metadata().valid_bits == 16 &&
                reader.metadata().channel_mask == 0,
            "non-extensible headers expose container width as valid bits");
}
void rejects_malformed_extensible_headers() {
    const std::string data("\0\0\0\0\0\0", 6);
    ExtensibleFormat short_chunk; short_chunk.fmt_length = 18; short_chunk.cb_size = 0;
    require(decode_throws(extensible_wav(short_chunk, data)), "short extensible format chunk is rejected");
    ExtensibleFormat short_extension; short_extension.cb_size = 10;
    require(decode_throws(extensible_wav(short_extension, data)), "short cbSize extension is rejected");
    ExtensibleFormat unknown_subtype; unknown_subtype.subtype = 2;
    require(decode_throws(extensible_wav(unknown_subtype, data)), "compressed subtype is rejected");
    ExtensibleFormat too_many_valid_bits; too_many_valid_bits.valid_bits = 25;
    require(decode_throws(extensible_wav(too_many_valid_bits, data)), "valid bits above container are rejected");
    ExtensibleFormat zero_valid_bits; zero_valid_bits.valid_bits = 0;
    require(decode_throws(extensible_wav(zero_valid_bits, data)), "zero valid bits are rejected");
    ExtensibleFormat oversized_mask; oversized_mask.channel_mask = 0x7;
    require(decode_throws(extensible_wav(oversized_mask, data)), "mask with more speakers than channels is rejected");
    ExtensibleFormat partial_float; partial_float.container_bits = 32; partial_float.valid_bits = 24;
    partial_float.subtype = 3;
    require(decode_throws(extensible_wav(partial_float, std::string(8, '\0'))),
            "float with fewer valid bits than its container is rejected");
    require(decode_throws(pcm16_two_format_chunks_wav()), "duplicate format chunks are rejected");
}
} // namespace

int main() {
    decodes_pcm16_metadata_and_samples();
    rejects_non_wave_input();
    decodes_signed_24_bit_minimum();
    decodes_ieee_float32_samples();
    streaming_reader_returns_bounded_frame_batches();
    streaming_reader_continues_across_data_chunks();
    decodes_extensible_pcm_with_valid_bits_and_mask();
    decodes_extensible_float_with_unspecified_layout();
    legacy_formats_report_full_valid_bits_and_no_mask();
    rejects_malformed_extensible_headers();
    std::cout << "All WAV tests passed.\n";
}
