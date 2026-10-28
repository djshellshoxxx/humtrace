#include "humtrace/analysis.hpp"

#include "humtrace/sha256.hpp"
#include "humtrace/spectrum.hpp"
#include "humtrace/version.hpp"
#include "humtrace/wav.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

namespace humtrace {
namespace {
constexpr std::size_t kMaxInterleavedWindowSamples = 16 * 1024 * 1024;
constexpr std::size_t kMaxFrameSize = 65536;

bool is_power_of_two(std::size_t value) { return value != 0 && (value & (value - 1)) == 0; }

void check_cancel(const AnalysisOptions& options) {
    if (options.cancel != nullptr && options.cancel->load(std::memory_order_relaxed))
        throw AnalysisCancelled();
}

void hash_file(const std::string& path, AnalysisReport& report, const AnalysisOptions& options) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not open input file");
    Sha256 hash;
    std::vector<char> buffer(1 << 20);
    std::uint64_t total = 0;
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto got = static_cast<std::size_t>(input.gcount());
        hash.update(reinterpret_cast<const unsigned char*>(buffer.data()), got);
        total += got;
        check_cancel(options);
    }
    if (input.bad())
        throw std::runtime_error("failed while reading input file");
    report.input_sha256 = hash.hex_digest();
    report.input_size_bytes = total;
}

std::size_t choose_window(const WavMetadata& metadata, const AnalysisOptions& options) {
    const auto frames = metadata.frame_count;
    if (options.frame_size_samples != 0) {
        if (!is_power_of_two(options.frame_size_samples) || options.frame_size_samples < 4 ||
            options.frame_size_samples > kMaxFrameSize)
            throw std::runtime_error("frame size must be a power of two between 4 and 65536");
        if (options.frame_size_samples > frames)
            throw std::runtime_error("audio is shorter than the requested frame size");
        return options.frame_size_samples;
    }
    const auto limit = std::min<std::uint64_t>(
        frames, std::max<std::uint64_t>(4, std::min<std::uint64_t>(metadata.sample_rate_hz, kMaxFrameSize)));
    std::size_t window = 1;
    while (window <= limit / 2)
        window *= 2;
    return window;
}
} // namespace

AnalysisReport analyze_wav_file(const std::string& path, const AnalysisOptions& options) {
    AnalysisReport report;
    report.input_path = path;
    report.application_version = kVersion;
    report.support_threshold_dbfs = options.support_threshold_dbfs;
    report.minimum_prominence_db = options.minimum_prominence_db;
    if (!(options.minimum_prominence_db >= 0.0) || !(options.support_threshold_dbfs <= 0.0))
        throw std::runtime_error("threshold must be <= 0 dBFS and prominence >= 0 dB");

    hash_file(path, report, options);
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not open input file");
    WavStreamReader reader(input);
    const auto& metadata = reader.metadata();
    const auto frames = metadata.frame_count;
    if (frames < 4)
        throw std::runtime_error("audio is too short to analyze");
    const std::size_t window_size = choose_window(metadata, options);
    const std::size_t hop_size = window_size / 2;
    if (window_size > kMaxInterleavedWindowSamples / metadata.channels)
        throw std::runtime_error("too many channels for the selected analysis window");
    const auto windows = (frames - window_size) / hop_size + 1;
    if (windows > options.max_result_frames / metadata.channels)
        throw std::runtime_error("recording too long for the result limit; choose a larger frame size");

    report.sample_rate_hz = metadata.sample_rate_hz;
    report.channels = metadata.channels;
    report.bit_depth = metadata.bit_depth;
    report.format_code = metadata.format_code;
    report.duration_seconds = static_cast<double>(frames) / metadata.sample_rate_hz;
    report.frame_size_samples = window_size;
    report.hop_size_samples = hop_size;
    if ((frames - window_size) % hop_size != 0)
        report.warnings.push_back("trailing partial window shorter than one frame was not analyzed");
    if (metadata.sample_rate_hz < 200)
        report.warnings.push_back("sample rate is too low to resolve 50/60 Hz harmonics meaningfully");

    report.channel_results.resize(metadata.channels);
    for (std::uint16_t c = 0; c < metadata.channels; ++c) {
        report.channel_results[c].channel_number = static_cast<std::uint16_t>(c + 1);
        report.channel_results[c].frames.reserve(windows);
    }
    auto window = reader.read_frames(window_size);
    const auto samples_per_window = window_size * metadata.channels;
    if (window.size() != samples_per_window)
        throw std::runtime_error("could not read the first complete analysis window");
    std::uint64_t window_start = 0;
    std::vector<double> samples(window_size);
    while (true) {
        check_cancel(options);
        for (std::uint16_t c = 0; c < metadata.channels; ++c) {
            for (std::size_t f = 0; f < window_size; ++f)
                samples[f] = window[f * metadata.channels + c];
            auto measured = analyze_interference_timeline(
                samples, metadata.sample_rate_hz, window_size, window_size,
                options.support_threshold_dbfs, 10, options.minimum_prominence_db);
            if (measured.empty())
                throw std::runtime_error("analysis produced no complete frames");
            measured[0].start_sample = window_start;
            report.channel_results[c].frames.push_back(std::move(measured[0]));
        }
        if (options.progress)
            options.progress(std::min(1.0, static_cast<double>(window_start + window_size) /
                                               static_cast<double>(frames)));
        if (window_start + hop_size + window_size > frames)
            break;
        const auto retained = hop_size * metadata.channels;
        std::move(window.begin() + static_cast<std::ptrdiff_t>(retained), window.end(), window.begin());
        auto next = reader.read_frames(hop_size);
        if (next.size() != retained) {
            report.warnings.push_back("input ended before the declared data length");
            break;
        }
        std::copy(next.begin(), next.end(), window.end() - static_cast<std::ptrdiff_t>(next.size()));
        window_start += hop_size;
    }
    return report;
}

std::vector<SpectrumBin> load_frame_spectrum(const std::string& path, std::uint16_t channel_number,
                                             std::uint64_t start_sample, std::size_t frame_size) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not open input file");
    WavStreamReader reader(input);
    const auto& m = reader.metadata();
    if (channel_number == 0 || channel_number > m.channels)
        throw std::runtime_error("channel out of range");
    if (!is_power_of_two(frame_size) || frame_size < 4 || frame_size > kMaxFrameSize ||
        start_sample > m.frame_count || frame_size > m.frame_count - start_sample)
        throw std::runtime_error("frame is outside the recording");
    for (std::uint64_t skipped = 0; skipped < start_sample;) {
        const auto step = static_cast<std::size_t>(std::min<std::uint64_t>(start_sample - skipped, 65536));
        if (reader.read_frames(step).size() != step * m.channels)
            throw std::runtime_error("input ended early");
        skipped += step;
    }
    const auto block = reader.read_frames(frame_size);
    if (block.size() != frame_size * m.channels)
        throw std::runtime_error("input ended early");
    std::vector<double> samples(frame_size);
    for (std::size_t f = 0; f < frame_size; ++f)
        samples[f] = block[f * m.channels + (channel_number - 1U)];
    return analyze_spectrum(samples, m.sample_rate_hz);
}

void write_file_safely(const std::string& path, const std::string& contents,
                       const std::string& input_path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path target(path);
    if (path.empty())
        throw std::runtime_error("no output path given");
    if (!input_path.empty() && fs::exists(target, ec) && fs::equivalent(target, fs::path(input_path), ec))
        throw std::runtime_error("refusing to overwrite the input recording");
    const fs::path temp = target.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("could not create output file");
        out << contents;
        out.flush();
        if (!out) {
            out.close();
            fs::remove(temp, ec);
            throw std::runtime_error("failed while writing output file");
        }
    }
    if (fs::exists(target, ec)) {
        fs::copy_file(target, fs::path(target.string() + ".bak"), fs::copy_options::overwrite_existing, ec);
        if (ec) {
            fs::remove(temp, ec);
            throw std::runtime_error("could not back up the existing output file");
        }
    }
    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        throw std::runtime_error("could not move the output file into place");
    }
}

} // namespace humtrace
