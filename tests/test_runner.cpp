#include "humtrace/runner.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void put16(std::ofstream& out, std::uint16_t v) { out.put(static_cast<char>(v & 0xff)); out.put(static_cast<char>(v >> 8)); }
void put32(std::ofstream& out, std::uint32_t v) { put16(out, static_cast<std::uint16_t>(v & 0xffff)); put16(out, static_cast<std::uint16_t>(v >> 16)); }

void write_mono_wav(const std::filesystem::path& path, double hz) {
    constexpr std::uint32_t rate = 4096, frames = rate * 2;
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4); put32(out, 36 + frames * 2); out.write("WAVEfmt ", 8);
    put32(out, 16); put16(out, 1); put16(out, 1); put32(out, rate); put32(out, rate * 2); put16(out, 2); put16(out, 16);
    out.write("data", 4); put32(out, frames * 2);
    for (std::uint32_t i = 0; i < frames; ++i)
        put16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(
            16000.0 * std::sin(2 * std::numbers::pi * hz * i / rate))));
}
} // namespace

int main() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / "humtrace-runner-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const auto wav = (dir / "tone.wav").string();
    write_mono_wav(wav, 100.0);

    humtrace::AnalysisOptions options;
    std::vector<double> progress;
    options.progress = [&](double f) { progress.push_back(f); };
    const auto report = humtrace::analyze_wav_file(wav, options);
    require(report.input_path == "tone.wav", "basename recorded by default");
    require(report.channel_results.at(0).tracks.size() == 1, "tone produces one track");
    require(!progress.empty() && progress.back() == 1.0, "progress reaches 100%");
    for (std::size_t i = 1; i < progress.size(); ++i)
        require(progress[i] >= progress[i - 1], "progress is monotonic");

    std::atomic<bool> cancel{true};
    options.cancel = &cancel;
    bool cancelled = false;
    try { (void)humtrace::analyze_wav_file(wav, options); } catch (const humtrace::AnalysisCancelled&) { cancelled = true; }
    require(cancelled, "pre-set cancel flag aborts analysis");

    options.cancel = nullptr;
    options.a4_reference_hz = -5.0;
    bool rejected = false;
    try { (void)humtrace::analyze_wav_file(wav, options); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid A4 reference rejected");

    const auto out = (dir / "report.json").string();
    humtrace::write_file_atomically(out, "first");
    humtrace::write_file_atomically(out, "second");
    std::ifstream in(out);
    std::string text;
    std::getline(in, text);
    require(text == "second", "atomic write replaces an existing report");
    require(!fs::exists(out + ".tmp"), "no temp file remains");
    bool failed = false;
    try { humtrace::write_file_atomically((dir / "missing" / "r.json").string(), "x"); } catch (const std::runtime_error&) { failed = true; }
    require(failed, "unwritable destination reports an error");
    fs::remove_all(dir);
    std::cout << "All runner tests passed.\n";
}
