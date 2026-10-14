#include "humtrace/wav.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
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
std::string extensible_wav(std::uint16_t sub_format, std::uint16_t valid_bits) {
    std::string body;
    tag(body, "fmt "); u32(body, 40); u16(body, 0xFFFE); u16(body, 1);
    u32(body, 8000); u32(body, 16000); u16(body, 2); u16(body, 16);
    u16(body, 22); u16(body, valid_bits); u32(body, 4);
    u16(body, sub_format);
    const unsigned char guid_tail[14] = {0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xAA, 0, 0x38, 0x9B, 0x71};
    body.append(reinterpret_cast<const char*>(guid_tail), sizeof(guid_tail));
    tag(body, "data"); u32(body, 2); u16(body, 16384);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
void decodes_extensible_pcm_and_rejects_bad_subformat() {
    std::istringstream good(extensible_wav(1, 16), std::ios::binary);
    const auto audio = humtrace::decode_wav(good);
    require(audio.format_code == 1 && audio.interleaved_samples.size() == 1,
            "extensible PCM resolves to its sub-format");
    std::istringstream bad(extensible_wav(0x55, 16), std::ios::binary);
    bool threw = false;
    try { (void)humtrace::decode_wav(bad); } catch (const std::runtime_error&) { threw = true; }
    require(threw, "unsupported extensible sub-format is rejected");
    std::istringstream padded(extensible_wav(1, 12), std::ios::binary);
    threw = false;
    try { (void)humtrace::decode_wav(padded); } catch (const std::runtime_error&) { threw = true; }
    require(threw, "partial valid-bits extensible input is rejected");
}
std::string unpadded_odd_final_chunk_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
    u32(body, 8000); u32(body, 8000); u16(body, 1); u16(body, 8);
    tag(body, "data"); u32(body, 3); body.append("\x80\x90\x70", 3); // odd size, no pad byte
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
void accepts_missing_pad_on_final_odd_chunk() {
    std::istringstream stream(unpadded_odd_final_chunk_wav(), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.interleaved_samples.size() == 3, "all samples of an unpadded final chunk are decoded");
    auto bytes = unpadded_odd_final_chunk_wav() + std::string("\0\0\0", 3);
    bytes[4] = static_cast<char>(bytes[4] + 3);
    std::istringstream trailing(bytes, std::ios::binary);
    require(humtrace::decode_wav(trailing).interleaved_samples.size() == 3, "a few trailing bytes are ignored");
}
std::string raw_wav(std::uint16_t format, std::uint16_t bits, const std::string& samples) {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, format); u16(body, 1);
    u32(body, 8000); u32(body, static_cast<std::uint32_t>(8000 * bits / 8)); u16(body, static_cast<std::uint16_t>(bits / 8)); u16(body, bits);
    tag(body, "data"); u32(body, static_cast<std::uint32_t>(samples.size())); body += samples;
    if (samples.size() & 1U) body.push_back('\0');
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
bool decode_fails(const std::string& bytes) {
    std::istringstream in(bytes, std::ios::binary);
    try { (void)humtrace::decode_wav(in); } catch (const std::runtime_error&) { return true; }
    return false;
}
void decodes_exact_values_for_every_encoding() {
    { // 8-bit PCM is unsigned with 128 = silence
        std::istringstream in(raw_wav(1, 8, std::string("\x80\xff\x00\xc0", 4)), std::ios::binary);
        const auto a = humtrace::decode_wav(in).interleaved_samples;
        require(a[0] == 0.0 && std::abs(a[1] - 127.0 / 128.0) < 1e-12 && a[2] == -1.0 && a[3] == 0.5, "8-bit PCM offset and scale");
    }
    { // 32-bit PCM and float64 values
        std::string s32;
        u32(s32, 0x40000000U); u32(s32, 0x80000000U);
        std::istringstream in32(raw_wav(1, 32, s32), std::ios::binary);
        const auto a = humtrace::decode_wav(in32).interleaved_samples;
        require(a[0] == 0.5 && a[1] == -1.0, "32-bit PCM scale");
        const double v = 0.3125;
        std::uint64_t bits;
        std::memcpy(&bits, &v, 8);
        std::string s64;
        u32(s64, static_cast<std::uint32_t>(bits & 0xffffffffU)); u32(s64, static_cast<std::uint32_t>(bits >> 32));
        std::istringstream in64(raw_wav(3, 64, s64), std::ios::binary);
        require(humtrace::decode_wav(in64).interleaved_samples[0] == v, "float64 value preserved exactly");
    }
    { // non-finite float samples are rejected
        std::string nan32;
        u32(nan32, 0x7fc00000U);
        require(decode_fails(raw_wav(3, 32, nan32)), "NaN float sample rejected");
        std::string inf32;
        u32(inf32, 0x7f800000U);
        require(decode_fails(raw_wav(3, 32, inf32)), "infinite float sample rejected");
    }
}
void rejects_duplicate_or_late_format_chunks_and_chunk_floods() {
    std::string fmt1; tag(fmt1, "fmt "); u32(fmt1, 16); u16(fmt1, 1); u16(fmt1, 1); u32(fmt1, 8000); u32(fmt1, 16000); u16(fmt1, 2); u16(fmt1, 16);
    std::string fmt2 = fmt1;
    fmt2[10] = 2; // stereo
    std::string data; tag(data, "data"); u32(data, 8); data += std::string(8, '\x10');
    auto wrap = [](const std::string& body) { std::string r; tag(r, "RIFF"); u32(r, static_cast<std::uint32_t>(body.size() + 4)); tag(r, "WAVE"); return r + body; };
    require(decode_fails(wrap(fmt1 + data + fmt2)), "format chunk after the data is rejected");
    require(decode_fails(wrap(fmt1 + fmt2 + data)), "second format chunk is rejected");
    std::string flood = fmt1 + data;
    for (int i = 0; i < 20; ++i) { std::string empty; tag(empty, "data"); u32(empty, 0); flood += empty; }
    std::istringstream in(wrap(flood), std::ios::binary);
    require(humtrace::decode_wav(in).interleaved_samples.size() == 4, "empty data chunks are skipped, real data still decoded");
}
} // namespace

int main() {
    decodes_exact_values_for_every_encoding();
    rejects_duplicate_or_late_format_chunks_and_chunk_floods();
    accepts_missing_pad_on_final_odd_chunk();
    decodes_extensible_pcm_and_rejects_bad_subformat();
    decodes_pcm16_metadata_and_samples();
    rejects_non_wave_input();
    decodes_signed_24_bit_minimum();
    decodes_ieee_float32_samples();
    streaming_reader_returns_bounded_frame_batches();
    streaming_reader_continues_across_data_chunks();
    std::cout << "All WAV tests passed.\n";
}
