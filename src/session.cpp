#include "humtrace/session.hpp"

#include "humtrace/sha256.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <ostream>
#include <streambuf>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace humtrace {

std::filesystem::path path_from_utf8(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

AnalysisResult analyze_file(const std::string& path, const PipelineSettings& settings,
                            const SessionCallbacks& callbacks, bool want_views, FrameSink* sink) {
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
        want_views ? &result.views : nullptr, sink, [&] { report_progress(Stage::Finalizing, 0.0); });
    std::error_code ec_after;
    if (std::filesystem::file_size(native, ec_after) != size_before ||
        std::filesystem::last_write_time(native, ec_after) != time_before)
        throw std::runtime_error("input file changed during analysis; results discarded");
    result.report.input_path = path;
    result.report.input_bytes = size_before;
    result.report.input_sha256 = digest;
    report_progress(Stage::Done, 1.0);
    return result;
}

void require_distinct_paths(const std::string& output, const std::string& input) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto out = path_from_utf8(output);
    const auto in = path_from_utf8(input);
    bool same = fs::exists(out, ec) && fs::exists(in, ec) && fs::equivalent(out, in, ec);
    if (!same) {
        const auto a = fs::weakly_canonical(out, ec);
        const auto b = fs::weakly_canonical(in, ec);
        same = !ec && a == b;
    }
    if (same)
        throw std::runtime_error("refusing to overwrite the input recording with an export");
}

namespace {
class FileBuffer final : public std::streambuf {
public:
    explicit FileBuffer(std::FILE* file) : file_(file) {}
private:
    std::streamsize xsputn(const char* data, std::streamsize size) override {
        return static_cast<std::streamsize>(std::fwrite(data, 1, static_cast<std::size_t>(size), file_));
    }
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        return std::fputc(traits_type::to_char_type(value), file_) == EOF ? traits_type::eof() : value;
    }
    int sync() override { return std::fflush(file_); }
    std::FILE* file_;
};
}

void write_file_atomically(const std::string& path, const std::function<void(std::ostream&)>& writer, bool replace_existing) {
    std::error_code error_ignored;
    const auto target = path_from_utf8(path);
    // Exclusively create the sibling temp file: randomness alone does not make
    // truncating creation safe against collisions or pre-existing symlinks.
    std::random_device entropy;
    std::filesystem::path temp;
    std::FILE* out = nullptr;
    for (int attempt = 0; attempt < 16; ++attempt) {
        temp = target;
        temp += ".humtrace-tmp-" + std::to_string(entropy());
#ifdef _WIN32
        out = _wfopen(temp.c_str(), L"wbx");
#else
        out = std::fopen(temp.c_str(), "wbx");
#endif
        if (out != nullptr || errno != EEXIST) break;
    }
    if (out == nullptr)
        throw std::runtime_error("could not create output file");
    try {
        FileBuffer buffer(out);
        std::ostream output(&buffer);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        writer(output);
        output.flush();
    } catch (...) {
        std::fclose(out);
        std::filesystem::remove(temp, error_ignored);
        throw;
    }
    const bool closed = std::fclose(out) == 0;
    if (!closed) {
        std::filesystem::remove(temp, error_ignored);
        throw std::runtime_error("failed while writing output file");
    }
    std::error_code error;
#ifdef _WIN32
    // std::filesystem::rename cannot replace an existing destination on Windows.
    // Use the native same-volume replacement operation for --force / GUI saves.
    const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace_existing ? MOVEFILE_REPLACE_EXISTING : 0U);
    if (!MoveFileExW(temp.c_str(), target.c_str(), flags))
        error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    if (replace_existing) {
        std::filesystem::rename(temp, target, error);
    } else {
        // Linking publishes the completed file without replacing a concurrently
        // created destination. The sibling temporary is on the same filesystem.
        std::filesystem::create_hard_link(temp, target, error);
        if (!error) std::filesystem::remove(temp, error_ignored);
    }
#endif
    if (error) {
        std::filesystem::remove(temp, error_ignored);
        throw std::runtime_error("could not finalize output file: " + error.message());
    }
}

void write_file_atomically(const std::string& path, const std::string& content, bool replace_existing) {
    write_file_atomically(path, [&](std::ostream& output) {
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }, replace_existing);
}

} // namespace humtrace
