// Deterministic mutation fuzz: malformed WAV bytes must throw std::exception, never crash,
// hang, or over-allocate. Run under ASan/UBSan in CI.
#include "humtrace/wav.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace {
void u16(std::string& b, std::uint16_t v) { b.push_back(static_cast<char>(v & 0xff)); b.push_back(static_cast<char>(v >> 8)); }
void u32(std::string& b, std::uint32_t v) { for (unsigned s = 0; s < 32; s += 8) b.push_back(static_cast<char>((v >> s) & 0xff)); }
std::string seed_wav() {
    std::string data;
    for (int i = 0; i < 64; ++i) u16(data, static_cast<std::uint16_t>(i * 311));
    std::string body = "fmt ";
    u32(body, 16); u16(body, 1); u16(body, 2); u32(body, 8000); u32(body, 32000); u16(body, 4); u16(body, 16);
    body += "data"; u32(body, static_cast<std::uint32_t>(data.size())); body += data;
    std::string out = "RIFF";
    u32(out, static_cast<std::uint32_t>(body.size() + 4));
    return out + "WAVE" + body;
}
} // namespace

int main() {
    const std::string seed = seed_wav();
    std::mt19937 rng(12345);
    std::size_t accepted = 0, rejected = 0;
    for (int iteration = 0; iteration < 20000; ++iteration) {
        std::string bytes = seed;
        const int edits = 1 + static_cast<int>(rng() % 4);
        for (int e = 0; e < edits; ++e) {
            const auto position = rng() % bytes.size();
            switch (rng() % 3) {
            case 0: bytes[position] = static_cast<char>(rng()); break;
            case 1: bytes.resize(position); if (bytes.empty()) bytes = "R"; break;
            default: bytes.insert(position, 1, static_cast<char>(rng())); break;
            }
        }
        try {
            std::istringstream in(bytes, std::ios::binary);
            const auto audio = humtrace::decode_wav(in);
            if (audio.channels == 0 || audio.interleaved_samples.size() % audio.channels != 0) {
                std::cerr << "FAIL: accepted inconsistent audio\n";
                return 1;
            }
            ++accepted;
        } catch (const std::exception&) {
            ++rejected;
        }
    }
    std::cout << "WAV fuzz: " << accepted << " accepted, " << rejected << " rejected.\nAll WAV fuzz tests passed.\n";
}
