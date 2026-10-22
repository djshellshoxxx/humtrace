#include "humtrace/streaming.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>

namespace fs = std::filesystem;
namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void integer(std::ostream& out, std::uint32_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 255));
}
}
int main() {
    std::random_device random;
    const auto directory = fs::temp_directory_path() / ("humtrace-stream-api-" + std::to_string(random()));
    require(fs::create_directory(directory), "private test directory created");
    const auto input = directory / "input.wav";
    {
        std::ofstream out(input, std::ios::binary);
        out << "RIFF"; integer(out, 36 + 32768, 4); out << "WAVEfmt "; integer(out, 16, 4);
        integer(out, 1, 2); integer(out, 1, 2); integer(out, 4096, 4); integer(out, 8192, 4);
        integer(out, 2, 2); integer(out, 16, 2); out << "data"; integer(out, 32768, 4);
        for (int i = 0; i < 16384; ++i) integer(out, 0, 2);
    }
    const auto output = directory / "output.json";
    for (const auto stage : {humtrace::Stage::Hashing, humtrace::Stage::Analyzing, humtrace::Stage::Finalizing}) {
        bool cancel = false;
        humtrace::SessionCallbacks callbacks;
        callbacks.progress = [&](humtrace::Stage current, double) { if (current == stage) cancel = true; };
        callbacks.cancelled = [&] { return cancel; };
        bool cancelled = false;
        try { (void)humtrace::stream_file_report(input.string(), output.string(), {}, callbacks); }
        catch (const humtrace::AnalysisCancelled&) { cancelled = true; }
        require(cancelled, "cancel supported in hash, analysis and report finalization");
        require(!fs::exists(output), "cancellation never publishes partial report");
        std::size_t entries = 0;
        for (const auto& entry : fs::directory_iterator(directory)) { (void)entry; ++entries; }
        require(entries == 1, "all scratch and sibling temporary files removed");
    }
    humtrace::StreamingLimits limits;
    limits.min_free_disk_bytes = std::numeric_limits<std::uint64_t>::max();
    bool refused = false;
    try { (void)humtrace::stream_file_report(input.string(), output.string(), {}, {}, limits); }
    catch (const std::runtime_error& error) { refused = std::string(error.what()).find("free disk") != std::string::npos; }
    require(refused && !fs::exists(output), "insufficient free space is rejected before report publication");
    fs::remove_all(directory);
    std::cout << "Streaming API: cancellation at all stages, insufficient disk and cleanup passed.\n";
}
