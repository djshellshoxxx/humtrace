// Deterministic mutation fuzz of the WAV parser/analysis path. Any outcome is
// acceptable except a crash, hang, or sanitizer report; errors must be exceptions.
#include "humtrace/analysis.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace {
void le(std::string& s, std::uint32_t v, int n) { for (int i = 0; i < n; ++i) s.push_back(char((v >> (8 * i)) & 255)); }
std::string seed_wav(unsigned channels, unsigned bits) {
    std::string d;
    const unsigned frames = 2048;
    for (unsigned i = 0; i < frames * channels; ++i) {
        const auto v = std::lround(0.4 * 32767 * std::sin(0.05 * i));
        le(d, std::uint32_t(v), int(bits / 8));
    }
    std::string w = "RIFF"; le(w, 36 + std::uint32_t(d.size()), 4); w += "WAVEfmt ";
    le(w, 16, 4); le(w, 1, 2); le(w, channels, 2); le(w, 8000, 4); le(w, 8000 * channels * bits / 8, 4);
    le(w, channels * bits / 8, 2); le(w, bits, 2); w += "data"; le(w, std::uint32_t(d.size()), 4);
    return w + d;
}
}

int main() {
    namespace fs = std::filesystem;
    const auto path = (fs::temp_directory_path() / "humtrace-fuzz.wav").string();
    std::mt19937 rng(12345);
    unsigned accepted = 0, rejected = 0;
    for (int iter = 0; iter < 600; ++iter) {
        auto bytes = seed_wav(1 + unsigned(iter % 2), (iter % 3 == 0) ? 16 : (iter % 3 == 1) ? 24 : 8);
        const int mutations = 1 + int(rng() % 4);
        for (int m = 0; m < mutations; ++m) {
            switch (rng() % 4) {
            case 0: bytes[rng() % std::min<std::size_t>(bytes.size(), 64)] = char(rng()); break;  // header bytes
            case 1: bytes[rng() % bytes.size()] = char(rng()); break;
            case 2: bytes.resize(rng() % bytes.size()); break;
            default: bytes.insert(rng() % bytes.size(), std::string(rng() % 16, char(rng()))); break;
            }
            if (bytes.empty()) bytes = "x";
        }
        std::ofstream(path, std::ios::binary | std::ios::trunc).write(bytes.data(), std::streamsize(bytes.size()));
        try { (void)humtrace::analyze_wav_file(path); ++accepted; }
        catch (const std::runtime_error&) { ++rejected; }
        catch (const std::invalid_argument&) { ++rejected; }
    }
    fs::remove(path);
    std::cout << "fuzz ok: accepted " << accepted << ", rejected " << rejected << '\n';
}
