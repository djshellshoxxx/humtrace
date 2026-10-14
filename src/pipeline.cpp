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
                           const std::function<void(double)>& progress,
                           const std::function<bool()>& cancelled,
                           std::vector<ChannelView>* views) {
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
    report.support_threshold_dbfs = settings.support_threshold_dbfs;
    report.minimum_prominence_db = settings.minimum_prominence_db;
    report.channel_results.resize(metadata.channels);
    for (std::uint16_t channel = 0; channel < metadata.channels; ++channel)
        report.channel_results[channel].channel_number = static_cast<std::uint16_t>(channel + 1);

    const auto samples_per_window = window_size * metadata.channels;
    auto window = reader.read_frames(window_size);
    if (window.size() != samples_per_window)
        throw std::runtime_error("could not read the first complete analysis window");

    std::vector<double> samples(window_size);
    std::vector<double> samples_new;
    std::vector<std::vector<FramePeaks>> peaks(metadata.channels);
    std::vector<ChannelView> local_views(metadata.channels);
    std::vector<EnvelopeBuilder> envelopes;
    const auto total_per_channel = metadata.frame_count;
    for (std::uint16_t channel = 0; channel < metadata.channels; ++channel)
        envelopes.emplace_back(settings.waveform_buckets, total_per_channel);
    std::vector<double> row_edges;
    std::uint64_t window_start = 0;
    while (true) {
        if (cancelled && cancelled())
            throw AnalysisCancelled();
        for (std::uint16_t channel = 0; channel < metadata.channels; ++channel) {
            for (std::size_t i = 0; i < window_size; ++i)
                samples[i] = window[i * metadata.channels + channel];
            const auto spectrum = analyze_spectrum(samples, metadata.sample_rate_hz);
            if (row_edges.empty()) {
                row_edges = log_row_edges(spectrum, settings.spectrogram_rows, settings.spectrogram_min_hz);
                for (auto& view : local_views)
                    view.row_frequency_hz.assign(row_edges.begin(), row_edges.end() - 1);
            }
            local_views[channel].spectrogram_dbfs.push_back(
                reduce_to_rows(spectrum, row_edges, settings.spectrogram_floor_dbfs));
            // The first window contributes all samples; later windows only their new hop.
            const auto skip = window_start == 0 ? std::size_t{0} : hop_size;
            for (std::size_t i = skip; i < window_size; ++i)
                samples_new.push_back(samples[i]);
            envelopes[channel].add(samples_new.data(), samples_new.size());
            samples_new.clear();
            report.channel_results[channel].frames.push_back(
                measure_frame(spectrum, window_start, report.support_threshold_dbfs,
                              10, report.minimum_prominence_db));
            FramePeaks entry{window_start, {}};
            for (const auto& peak : find_peaks(spectrum, settings.peak_min_level_dbfs,
                                               settings.peak_min_prominence_db,
                                               settings.max_peaks_per_frame))
                entry.peaks.push_back({peak.frequency_hz, peak.level_dbfs});
            peaks[channel].push_back(std::move(entry));
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
    for (std::size_t c = 0; c < peaks.size(); ++c)
        report.channel_results[c].tracks = track_peaks(peaks[c], metadata.sample_rate_hz, tracker);
    for (std::size_t c = 0; c < local_views.size(); ++c)
        envelopes[c].finish(local_views[c].waveform_min, local_views[c].waveform_max);
    if (views)
        *views = std::move(local_views);
    return report;
}

} // namespace humtrace
