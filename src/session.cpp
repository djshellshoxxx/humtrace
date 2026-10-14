#include "humtrace/session.hpp"

#include "humtrace/sha256.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace humtrace {

AnalysisResult analyze_file(const std::string& path, const PipelineSettings& settings,
                            const SessionCallbacks& callbacks, bool want_views) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not open input file");
    auto report_progress = [&](Stage stage, double fraction) {
        if (callbacks.progress)
            callbacks.progress(stage, fraction);
    };
    if (callbacks.cancelled && callbacks.cancelled())
        throw AnalysisCancelled();
    report_progress(Stage::Hashing, 0.0);
    const auto digest = sha256_hex(input);
    report_progress(Stage::Hashing, 1.0);
    input.clear();
    input.seekg(0);
    WavStreamReader reader(input);
    AnalysisResult result;
    result.report = analyze_wav(
        reader, settings, [&](double f) { report_progress(Stage::Analyzing, f); }, callbacks.cancelled,
        want_views ? &result.views : nullptr);
    result.report.input_path = path;
    result.report.input_sha256 = digest;
    report_progress(Stage::Done, 1.0);
    return result;
}

void write_file_atomically(const std::string& path, const std::string& content) {
    const std::string temp = path + ".humtrace-tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("could not create output file");
        out << content;
        out.flush();
        if (!out) {
            out.close();
            std::remove(temp.c_str());
            throw std::runtime_error("failed while writing output file");
        }
    }
    std::error_code error;
    std::filesystem::rename(temp, path, error);
    if (error) {
        std::remove(temp.c_str());
        throw std::runtime_error("could not finalize output file: " + error.message());
    }
}

} // namespace humtrace
