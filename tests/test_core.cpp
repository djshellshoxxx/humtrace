#include "humtrace/analysis.hpp"
#include "humtrace/compare.hpp"
#include "humtrace/sha256.hpp"

#include <atomic>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

std::string sha(const std::string& text) {
    humtrace::Sha256 h;
    h.update(reinterpret_cast<const unsigned char*>(text.data()), text.size());
    return h.hex_digest();
}

void put16(std::string& s, std::uint16_t v) { s.push_back(char(v & 255)); s.push_back(char(v >> 8)); }
void put32(std::string& s, std::uint32_t v) { put16(s, v & 0xffff); put16(s, std::uint16_t(v >> 16)); }

// 16-bit mono sine WAV. `declared_extra` lies about the data length (truncation test).
std::string make_wav(double hz, unsigned rate, unsigned frames, std::uint32_t declared_extra = 0) {
    std::string data;
    for (unsigned i = 0; i < frames; ++i)
        put16(data, std::uint16_t(std::int16_t(std::lround(0.5 * 32767 * std::sin(2 * std::numbers::pi * hz * i / rate)))));
    std::string w = "RIFF";
    put32(w, 36 + std::uint32_t(data.size()) + declared_extra);
    w += "WAVEfmt ";
    put32(w, 16); put16(w, 1); put16(w, 1); put32(w, rate); put32(w, rate * 2); put16(w, 2); put16(w, 16);
    w += "data";
    put32(w, std::uint32_t(data.size()) + declared_extra);
    return w + data;
}

fs::path write_tmp(const fs::path& dir, const std::string& name, const std::string& bytes) {
    const auto p = dir / name;
    std::ofstream(p, std::ios::binary) << bytes;
    return p;
}

void sha256_known_vectors() {
    require(sha("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty");
    require(sha("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 abc");
    require(sha(std::string(1000000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "sha256 million a");
    require(sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "sha256 two-block");
}

void analyzes_synthetic_tone_and_hashes_input(const fs::path& dir) {
    const auto bytes = make_wav(60.0, 4096, 4096 * 2);
    const auto path = write_tmp(dir, "tone.wav", bytes);
    int calls = 0;
    humtrace::AnalysisOptions o;
    o.progress = [&](double f) { require(f >= 0 && f <= 1, "progress in range"); ++calls; };
    const auto r = humtrace::analyze_wav_file(path.string(), o);
    require(r.input_sha256 == sha(bytes), "report hash matches input bytes");
    require(r.input_size_bytes == bytes.size(), "report size matches");
    require(calls == 3, "progress reported per window");
    require(std::abs(r.channel_results[0].frames[0].strongest_peak.frequency_hz - 60.0) < 0.2, "60 Hz measured");
    const auto s = humtrace::summarize(r, 1);
    require(s.channels.size() == 1 && s.channels[0].frames == 3, "summary frame count");
    require(humtrace::format_comparison({s}).find("sha256 ") != std::string::npos, "comparison lists hash");
    require(humtrace::format_comparison({s}) == humtrace::format_comparison({humtrace::summarize(r, 1)}), "deterministic");
}

void cancellation_stops_analysis(const fs::path& dir) {
    const auto path = write_tmp(dir, "c.wav", make_wav(60, 4096, 4096 * 4));
    std::atomic<bool> cancel{true};
    humtrace::AnalysisOptions o;
    o.cancel = &cancel;
    bool threw = false;
    try { (void)humtrace::analyze_wav_file(path.string(), o); } catch (const humtrace::AnalysisCancelled&) { threw = true; }
    require(threw, "cancel flag aborts analysis");
}

void rejects_bad_input(const fs::path& dir) {
    auto fails = [&](const std::string& file, humtrace::AnalysisOptions o = {}) {
        try { (void)humtrace::analyze_wav_file(file, o); } catch (const std::runtime_error&) { return true; }
        return false;
    };
    require(fails((dir / "missing.wav").string()), "missing file");
    require(fails(write_tmp(dir, "empty.wav", "").string()), "empty file");
    require(fails(write_tmp(dir, "short.wav", make_wav(60, 4096, 2)).string()), "too short");
    const auto ok = write_tmp(dir, "ok.wav", make_wav(60, 4096, 8192)).string();
    humtrace::AnalysisOptions o;
    o.frame_size_samples = 1000;
    require(fails(ok, o), "non power-of-two frame size");
    o.frame_size_samples = 1 << 20;
    require(fails(ok, o), "oversized frame size");
    o = {}; o.support_threshold_dbfs = 5.0;
    require(fails(ok, o), "positive threshold");
    o = {}; o.max_result_frames = 1;
    require(fails(ok, o), "result cap enforced");
    // Regression: header claims more data than the file holds must not crash or hang.
    const auto lie = write_tmp(dir, "lie.wav", make_wav(60, 4096, 8192, 1u << 30)).string();
    try { const auto r = humtrace::analyze_wav_file(lie); (void)r; } catch (const std::runtime_error&) {}
    std::string trunc = make_wav(60, 4096, 8192);
    trunc.resize(trunc.size() - 3000);
    try { (void)humtrace::analyze_wav_file(write_tmp(dir, "trunc.wav", trunc).string()); } catch (const std::runtime_error&) {}
}

void safe_write_protects_input_and_backs_up(const fs::path& dir) {
    const auto in = write_tmp(dir, "in.wav", "RIFFdata");
    bool threw = false;
    try { humtrace::write_file_safely(in.string(), "x", in.string()); } catch (const std::runtime_error&) { threw = true; }
    require(threw, "refuses to overwrite input");
    require(fs::file_size(in) == 8, "input untouched");
    const auto out = dir / "r.json";
    humtrace::write_file_safely(out.string(), "one", in.string());
    humtrace::write_file_safely(out.string(), "two", in.string());
    std::string a, b;
    std::ifstream(out) >> a; std::ifstream(out.string() + ".bak") >> b;
    require(a == "two" && b == "one", "previous report backed up");
    require(!fs::exists(out.string() + ".tmp"), "no temp file left");
    // Regression: a pre-planted symlink at <out>.tmp must not be written through.
    const auto victim = write_tmp(dir, "victim.txt", "keep");
    const auto out2 = dir / "r2.json";
    fs::create_symlink(victim, out2.string() + ".tmp");
    threw = false;
    try { humtrace::write_file_safely(out2.string(), "evil", in.string()); } catch (const std::runtime_error&) { threw = true; }
    std::string kept; std::ifstream(victim) >> kept;
    require(threw && kept == "keep", "symlinked temp path refused");
}
} // namespace

int main() {
    const auto dir = fs::temp_directory_path() / ("humtrace-core-" + std::to_string(std::rand()));
    fs::create_directories(dir);
    sha256_known_vectors();
    analyzes_synthetic_tone_and_hashes_input(dir);
    cancellation_stops_analysis(dir);
    rejects_bad_input(dir);
    safe_write_protects_input_and_backs_up(dir);
    fs::remove_all(dir);
    std::cout << "All core tests passed.\n";
}
