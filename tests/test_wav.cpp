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
} // namespace

int main() {
    decodes_pcm16_metadata_and_samples();
    rejects_non_wave_input();
    decodes_signed_24_bit_minimum();
    std::cout << "All WAV tests passed.\n";
}
