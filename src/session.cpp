#include "humtrace/session.hpp"

#include "humtrace/sha256.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace humtrace {

std::filesystem::path path_from_utf8(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

AnalysisResult analyze_file(const std::string& path, const PipelineSettings& settings,
                            const SessionCallbacks& callbacks, bool want_views) {
    const auto native = path_from_utf8(path);
    std::ifstream input(native, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not open input file");
    std::error_code ec;
    const auto size_before = std::filesystem::file_size(native, ec);
    const auto time_before = std::filesystem::last_write_time(native, ec);
    auto report_progress = [&](Stage stage, double fraction) {
        if (callbacks.progress)
            callbacks.progress(stage, fraction);
    };
    if (callbacks.cancelled && callbacks.cancelled())
        throw AnalysisCancelled();
    report_progress(Stage::Hashing, 0.0);
    input.seekg(0, std::ios::end);
    const auto size = static_cast<double>(std::max<std::streamoff>(1, input.tellg()));
    input.seekg(0);
    const auto hashed = sha256_hex_cancellable(
        input, callbacks.cancelled,
        [&](std::uint64_t bytes) { report_progress(Stage::Hashing, static_cast<double>(bytes) / size); });
    if (!hashed)
        throw AnalysisCancelled();
    const auto& digest = *hashed;
    report_progress(Stage::Hashing, 1.0);
    input.clear();
    input.seekg(0);
    WavStreamReader reader(input);
    AnalysisResult result;
    result.report = analyze_wav(
        reader, settings, [&](double f) { report_progress(Stage::Analyzing, f); }, callbacks.cancelled,
        want_views ? &result.views : nullptr);
    std::error_code ec_after;
    if (std::filesystem::file_size(native, ec_after) != size_before ||
        std::filesystem::last_write_time(native, ec_after) != time_before)
        throw std::runtime_error("input file changed during analysis; results discarded");
    result.report.input_path = path;
    result.report.input_sha256 = digest;
    report_progress(Stage::Done, 1.0);
    return result;
}

void write_file_atomically(const std::string& path, const std::string& content) {
    std::error_code error_ignored;
    const auto target = path_from_utf8(path);
    auto temp = target;
    temp += ".humtrace-tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("could not create output file");
        out << content;
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(temp, error_ignored);
            throw std::runtime_error("failed while writing output file");
        }
    }
    std::error_code error;
    std::filesystem::rename(temp, target, error);
    if (error) {
        std::filesystem::remove(temp, error_ignored);
        throw std::runtime_error("could not finalize output file: " + error.message());
    }
}

} // namespace humtrace
