#include "humtrace/session.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>

namespace fs = std::filesystem;
namespace {
void require(bool c, const char* m) { if (!c) { std::cerr << "FAIL: " << m << '\n'; std::exit(1); } }
void put(std::string& b, std::uint32_t v, int bytes) { for (int i = 0; i < bytes; ++i) b.push_back(static_cast<char>((v >> (8 * i)) & 0xff)); }
std::string wav(std::uint32_t frames) {
    std::string data;
    for (std::uint32_t i = 0; i < frames; ++i)
        put(data, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(12000.0 * std::sin(2.0 * std::numbers::pi * 60.0 * i / 4096.0)))), 2);
    std::string body = "fmt ";
    put(body, 16, 4); put(body, 1, 2); put(body, 1, 2); put(body, 4096, 4); put(body, 8192, 4); put(body, 2, 2); put(body, 16, 2);
    body += "data"; put(body, static_cast<std::uint32_t>(data.size()), 4); body += data;
    std::string out = "RIFF";
    put(out, static_cast<std::uint32_t>(body.size() + 4), 4);
    return out + "WAVE" + body;
}
} // namespace

int main() {
    const auto dir = fs::temp_directory_path() / "humtrace-session-test";
    fs::create_directories(dir);
    { // non-ASCII (UTF-8) file names work for read and atomic write
        const std::string name = (dir / fs::path(u8"résumé-テスト.wav")).string();
        { std::ofstream f(humtrace::path_from_utf8(name), std::ios::binary); const auto w = wav(16384); f.write(w.data(), static_cast<std::streamsize>(w.size())); }
        const auto result = humtrace::analyze_file(name, {}, {}, false);
        require(result.report.channel_results[0].tracks.size() == 1, "analysis of a UTF-8 path");
        const std::string out = (dir / fs::path(u8"ümlaut.json")).string();
        humtrace::write_file_atomically(out, "{}");
        require(fs::exists(humtrace::path_from_utf8(out)), "UTF-8 output path written");
    }
    { // a file modified during analysis is rejected, not silently analyzed
        const auto path = (dir / "changing.wav").string();
        { std::ofstream f(path, std::ios::binary); const auto w = wav(16384); f.write(w.data(), static_cast<std::streamsize>(w.size())); }
        humtrace::SessionCallbacks cb;
        bool touched = false;
        cb.progress = [&](humtrace::Stage stage, double fraction) {
            if (stage == humtrace::Stage::Analyzing && fraction > 0.2 && !touched) {
                touched = true;
                std::ofstream f(path, std::ios::binary | std::ios::app);
                f << 'x';
            }
        };
        bool threw = false;
        try { (void)humtrace::analyze_file(path, {}, cb, false); }
        catch (const std::runtime_error& e) { threw = std::string(e.what()).find("changed during analysis") != std::string::npos; }
        require(threw, "mid-analysis modification detected");
    }
    { // missing input and cancelled-before-start
        bool threw = false;
        try { (void)humtrace::analyze_file((dir / "absent.wav").string(), {}, {}, false); } catch (const std::runtime_error&) { threw = true; }
        require(threw, "missing file rejected");
        const auto path = (dir / "c.wav").string();
        { std::ofstream f(path, std::ios::binary); const auto w = wav(8192); f.write(w.data(), static_cast<std::streamsize>(w.size())); }
        humtrace::SessionCallbacks cb;
        cb.cancelled = [] { return true; };
        threw = false;
        try { (void)humtrace::analyze_file(path, {}, cb, false); } catch (const humtrace::AnalysisCancelled&) { threw = true; }
        require(threw, "cancel before start");
    }
    { // exports may not overwrite the recording (same path, or same file through another spelling)
        const auto path = (dir / "keep.wav").string();
        { std::ofstream f(path, std::ios::binary); const auto w = wav(8192); f.write(w.data(), static_cast<std::streamsize>(w.size())); }
        bool threw = false;
        try { humtrace::require_distinct_paths(path, path); } catch (const std::runtime_error&) { threw = true; }
        require(threw, "identical paths refused");
        threw = false;
        try { humtrace::require_distinct_paths((dir / "sub" / ".." / "keep.wav").string(), path); } catch (const std::runtime_error&) { threw = true; }
        require(threw, "alternate spelling of the same file refused");
        humtrace::require_distinct_paths((dir / "other.json").string(), path); // distinct: no throw
    }
    fs::remove_all(dir);
    std::cout << "All session tests passed.\n";
}
