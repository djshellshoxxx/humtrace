#include "humtrace/runner.hpp"

#include "humtrace/music.hpp"
#include "humtrace/sha256.hpp"
#include "humtrace/wav.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace humtrace {
namespace {
void check_cancel(const AnalysisOptions& options) {
    if (options.cancel != nullptr && options.cancel->load())
        throw AnalysisCancelled();
}
} // namespace

AnalysisReport analyze_wav_file(const std::string& path, const AnalysisOptions& options) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto size_before = fs::file_size(path, ec);
    if (ec)
        throw std::runtime_error("could not open input file");
    const auto time_before = fs::last_write_time(path, ec);

    AnalysisReport report;
    report.input_path = options.record_full_path ? path : fs::path(path).filename().string();
    {
        std::ifstream hash_input(path, std::ios::binary);
        if (!hash_input)
            throw std::runtime_error("could not open input file");
        const auto digest = digest_stream(hash_input);
        report.input_sha256 = digest.sha256_hex;
        report.input_size_bytes = digest.bytes;
    }
    check_cancel(options);

    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not open input file");
    WavStreamReader reader(input);
    const auto& metadata = reader.metadata();
    const auto frames = metadata.frame_count;
    if (frames < 4)
        throw std::runtime_error("audio is too short to analyze");
    std::size_t window_size = 1;
    const auto frame_limit = std::min<std::uint64_t>(
        frames, std::max<std::uint64_t>(4, std::min<std::uint64_t>(metadata.sample_rate_hz, 65536)));
    while (window_size <= frame_limit / 2)
        window_size *= 2;
    const std::size_t hop_size = window_size / 2;
    constexpr std::size_t max_interleaved_window_samples = 16 * 1024 * 1024;
    if (window_size > max_interleaved_window_samples / metadata.channels)
        throw std::runtime_error("too many channels for the selected analysis window");
    const double sample_rate = metadata.sample_rate_hz;
    const double bin_spacing = sample_rate / static_cast<double>(window_size);

    report.sample_rate_hz = metadata.sample_rate_hz;
    report.channels = metadata.channels;
    report.bit_depth = metadata.bit_depth;
    report.format_code = metadata.format_code;
    report.duration_seconds = static_cast<double>(frames) / sample_rate;
    report.frame_size_samples = window_size;
    report.hop_size_samples = hop_size;
    report.peak_settings = options.peaks;
    report.tracker_settings = options.tracker;
    report.a4_reference_hz = options.a4_reference_hz;
    (void)describe_note(440.0, options.a4_reference_hz);  // validates the reference

    report.channel_results.resize(metadata.channels);
    std::vector<PeakTracker> trackers;
    for (std::uint16_t channel = 0; channel < metadata.channels; ++channel) {
        report.channel_results[channel].channel_number = static_cast<std::uint16_t>(channel + 1);
        trackers.emplace_back(options.tracker, bin_spacing, sample_rate);
    }

    auto interleaved_window = reader.read_frames(window_size);
    const auto samples_per_window = window_size * metadata.channels;
    if (interleaved_window.size() != samples_per_window)
        throw std::runtime_error("could not read the first complete analysis window");
    std::uint64_t window_start = 0;
    while (true) {
        check_cancel(options);
        for (std::uint16_t channel = 0; channel < metadata.channels; ++channel) {
            std::vector<double> samples(window_size);
            for (std::size_t frame = 0; frame < window_size; ++frame)
                samples[frame] = interleaved_window[frame * metadata.channels + channel];
            const auto spectrum = analyze_spectrum(samples, sample_rate);
            report.channel_results[channel].frames.push_back(
                {window_start, strongest_peak(spectrum),
                 measure_harmonics(spectrum, 50.0, report.support_threshold_dbfs, 10, report.minimum_prominence_db),
                 measure_harmonics(spectrum, 60.0, report.support_threshold_dbfs, 10, report.minimum_prominence_db)});
            trackers[channel].add_frame(window_start, find_local_peaks(spectrum, options.peaks));
        }
        if (options.progress)
            options.progress(std::min(1.0, static_cast<double>(window_start + window_size) /
                                               static_cast<double>(frames)));
        if (window_start + hop_size + window_size > frames)
            break;
        const auto retained_samples = hop_size * metadata.channels;
        std::move(interleaved_window.begin() + static_cast<std::ptrdiff_t>(retained_samples),
                  interleaved_window.end(), interleaved_window.begin());
        auto next_samples = reader.read_frames(hop_size);
        if (next_samples.size() != retained_samples)
            break;
        std::copy(next_samples.begin(), next_samples.end(),
                  interleaved_window.end() - static_cast<std::ptrdiff_t>(next_samples.size()));
        window_start += hop_size;
    }
    for (std::size_t channel = 0; channel < trackers.size(); ++channel)
        report.channel_results[channel].tracks = trackers[channel].finish();

    const auto size_after = fs::file_size(path, ec);
    const auto time_after = fs::last_write_time(path, ec);
    report.input_unchanged_during_analysis = size_after == size_before && time_after == time_before;
    return report;
}

void write_file_atomically(const std::string& path, const std::string& contents) {
    namespace fs = std::filesystem;
    const std::string temporary = path + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("could not create JSON report");
        out << contents;
        out.flush();
        if (!out) {
            out.close();
            std::error_code ignored;
            fs::remove(temporary, ignored);
            throw std::runtime_error("failed while writing JSON report");
        }
    }
    std::error_code ec;
    fs::rename(temporary, path, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw std::runtime_error("could not finalize JSON report: " + ec.message());
    }
}

} // namespace humtrace
