#include "humtrace/pipeline.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace humtrace {

std::size_t select_window_size(const WavMetadata& metadata,
                               std::size_t max_interleaved_window_samples) {
    if (metadata.frame_count < 4)
        throw std::runtime_error("audio is too short to analyze");
    const auto frame_limit = std::min<std::uint64_t>(
        metadata.frame_count,
        std::max<std::uint64_t>(4, std::min<std::uint64_t>(metadata.sample_rate_hz, 65536)));
    std::size_t window_size = 1;
    while (window_size <= frame_limit / 2)
        window_size *= 2;
    if (window_size > max_interleaved_window_samples / metadata.channels)
        throw std::runtime_error("too many channels for the selected analysis window");
    return window_size;
}

AnalysisReport analyze_wav(WavStreamReader& reader, const PipelineSettings& settings,
                           const std::function<void(double)>& progress) {
    const auto& metadata = reader.metadata();
    const auto frames = metadata.frame_count;
    const auto window_size = select_window_size(metadata, settings.max_interleaved_window_samples);
    const auto hop_size = window_size / 2;

    AnalysisReport report;
    report.sample_rate_hz = metadata.sample_rate_hz;
    report.channels = metadata.channels;
    report.bit_depth = metadata.bit_depth;
    report.format_code = metadata.format_code;
    report.duration_seconds = static_cast<double>(frames) / metadata.sample_rate_hz;
    report.frame_size_samples = window_size;
    report.hop_size_samples = hop_size;
    report.channel_results.resize(metadata.channels);
    for (std::uint16_t channel = 0; channel < metadata.channels; ++channel)
        report.channel_results[channel].channel_number = static_cast<std::uint16_t>(channel + 1);

    const auto samples_per_window = window_size * metadata.channels;
    auto window = reader.read_frames(window_size);
    if (window.size() != samples_per_window)
        throw std::runtime_error("could not read the first complete analysis window");

    std::vector<double> samples(window_size);
    std::uint64_t window_start = 0;
    while (true) {
        for (std::uint16_t channel = 0; channel < metadata.channels; ++channel) {
            for (std::size_t i = 0; i < window_size; ++i)
                samples[i] = window[i * metadata.channels + channel];
            auto measured = analyze_interference_timeline(
                samples, metadata.sample_rate_hz, window_size, window_size);
            if (measured.empty())
                throw std::runtime_error("analysis produced no complete frames");
            measured[0].start_sample = window_start;
            report.channel_results[channel].frames.push_back(std::move(measured[0]));
        }
        if (progress)
            progress(std::min(1.0, static_cast<double>(window_start + window_size) /
                                       static_cast<double>(frames)));
        if (window_start + hop_size + window_size > frames)
            break;
        const auto retained = hop_size * metadata.channels;
        std::move(window.begin() + static_cast<std::ptrdiff_t>(retained), window.end(), window.begin());
        auto next = reader.read_frames(hop_size);
        if (next.size() != retained)
            break;
        std::copy(next.begin(), next.end(), window.end() - static_cast<std::ptrdiff_t>(next.size()));
        window_start += hop_size;
    }

    auto tracker = settings.tracker;
    tracker.bin_spacing_hz = static_cast<double>(metadata.sample_rate_hz) / static_cast<double>(window_size);
    report.tracker = tracker;
    for (auto& channel : report.channel_results) {
        std::vector<FramePeaks> peaks;
        peaks.reserve(channel.frames.size());
        for (const auto& frame : channel.frames) {
            FramePeaks entry{frame.start_sample, {}};
            if (frame.strongest_peak.frequency_hz > 0.0)
                entry.peaks.push_back({frame.strongest_peak.frequency_hz, frame.strongest_peak.level_dbfs});
            peaks.push_back(std::move(entry));
        }
        channel.tracks = track_peaks(peaks, metadata.sample_rate_hz, tracker);
    }
    return report;
}

} // namespace humtrace
