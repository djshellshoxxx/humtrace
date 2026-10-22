#include "humtrace/pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace humtrace {

void validate_settings(const PipelineSettings& s) {
    auto finite = [](double v) { return std::isfinite(v); };
    if (!finite(s.support_threshold_dbfs) || !finite(s.peak_min_level_dbfs) || !finite(s.spectrogram_floor_dbfs))
        throw std::invalid_argument("level settings must be finite");
    if (!finite(s.minimum_prominence_db) || s.minimum_prominence_db < 0.0 ||
        !finite(s.peak_min_prominence_db) || s.peak_min_prominence_db < 0.0)
        throw std::invalid_argument("prominence settings must be finite and non-negative");
    if (!finite(s.spectrogram_min_hz) || s.spectrogram_min_hz <= 0.0)
        throw std::invalid_argument("spectrogram minimum frequency must be positive");
    if (s.max_peaks_per_frame > 1024 || s.waveform_buckets > (1U << 20) || s.spectrogram_rows > 4096)
        throw std::invalid_argument("peaks per frame (max 1024), waveform buckets (max 1048576) or spectrogram rows (max 4096) out of range");
    if (s.max_peaks_per_frame == 0 || s.waveform_buckets == 0 || s.spectrogram_rows == 0 ||
        s.tracker.min_observed_frames == 0 || s.max_interleaved_window_samples == 0 || s.max_result_bytes == 0)
        throw std::invalid_argument("count and size settings must be at least 1");
    if (s.welch.segment_size < 4 || s.welch.segment_size > max_welch_segment_size ||
        (s.welch.segment_size & (s.welch.segment_size - 1)) != 0 ||
        s.welch.hop_size == 0 || s.welch.hop_size > s.welch.segment_size)
        throw std::invalid_argument("invalid Welch segment or hop size");
    const auto& t = s.tracker;
    if (!finite(t.fixed_hz_tolerance) || t.fixed_hz_tolerance < 0.0 || !finite(t.relative_tolerance) ||
        t.relative_tolerance < 0.0 || !finite(t.bin_fraction) || t.bin_fraction < 0.0)
        throw std::invalid_argument("tracker tolerances must be finite and non-negative");
}

std::size_t select_window_size(const WavMetadata& metadata,
                               std::size_t max_interleaved_window_samples) {
    if (metadata.frame_count < 16)
        throw std::runtime_error("audio is too short to analyze");
    if (metadata.sample_rate_hz < 8)
        throw std::runtime_error("sample rate is too low to analyze");
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
    validate_settings(settings);
    const auto& metadata = reader.metadata();
    const auto frames = metadata.frame_count;
    const auto window_size = select_window_size(metadata, settings.max_interleaved_window_samples);
    const auto hop_size = window_size / 2;
    {
        const std::uint64_t windows = (frames - window_size) / hop_size + 2; // + final aligned window
        // Conservative per-window cost (values are bounded by validate_settings, so no overflow):
        // frame + harmonics, spectrogram row, peaks (observation + peak entry, ~64 B each) with 1.5x
        // vector growth, and bookkeeping.
        const std::uint64_t per_window = metadata.channels *
            (sizeof(InterferenceFrame) + 20 * sizeof(HarmonicMeasurement) +
             settings.spectrogram_rows * sizeof(float) + settings.max_peaks_per_frame * 64 * 3 / 2 + 128);
        const std::uint64_t psd_bytes = metadata.channels *
            (sizeof(PsdSummary) + 20 * sizeof(PsdHarmonicMeasurement));
        if (psd_bytes >= settings.max_result_bytes ||
            windows > (settings.max_result_bytes - psd_bytes) / per_window)
            throw std::runtime_error("recording is too long for the configured result memory limit");
    }

    // Each estimator retains a window, ring, FFT scratch and power accumulator. Account
    // for this separately from rolling FFT storage before allocating for many channels.
    if (settings.welch.segment_size > settings.max_interleaved_window_samples / metadata.channels)
        throw std::runtime_error("too many channels for the selected Welch segment");
    AnalysisReport report;
    report.welch = settings.welch;
    report.sample_rate_hz = metadata.sample_rate_hz;
    report.channels = metadata.channels;
    report.bit_depth = metadata.bit_depth;
    report.format_code = metadata.format_code;
    report.duration_seconds = static_cast<double>(frames) / metadata.sample_rate_hz;
    report.frame_size_samples = window_size;
    report.hop_size_samples = hop_size;
    report.peak_min_level_dbfs = settings.peak_min_level_dbfs;
    report.peak_min_prominence_db = settings.peak_min_prominence_db;
    report.max_peaks_per_frame = settings.max_peaks_per_frame;
    report.support_threshold_dbfs = settings.support_threshold_dbfs;
    report.minimum_prominence_db = settings.minimum_prominence_db;
    report.channel_results.resize(metadata.channels);
    for (std::uint16_t channel = 0; channel < metadata.channels; ++channel)
        report.channel_results[channel].channel_number = static_cast<std::uint16_t>(channel + 1);

    const auto samples_per_window = window_size * metadata.channels;
    auto window = reader.read_frames(window_size);
    if (window.size() != samples_per_window)
        throw std::runtime_error("could not read the first complete analysis window");

    std::vector<WelchPsd> estimators;
    estimators.reserve(metadata.channels);
    for (std::size_t c = 0; c < metadata.channels; ++c)
        estimators.emplace_back(metadata.sample_rate_hz, settings.welch);
    std::vector<double> samples(window_size);
    std::vector<std::vector<FramePeaks>> peaks(metadata.channels);
    std::vector<ChannelView> local_views(metadata.channels);
    std::vector<EnvelopeBuilder> envelopes;
    const auto total_per_channel = metadata.frame_count;
    for (std::uint16_t channel = 0; channel < metadata.channels; ++channel)
        envelopes.emplace_back(settings.waveform_buckets, total_per_channel);
    std::vector<double> row_edges;
    // Full scale for the stored bit depth (integer PCM tops out one step below 1.0).
    const double clip_threshold = metadata.format_code == 3 ? 1.0 : 1.0 - 1.5 * std::ldexp(1.0, -(metadata.bit_depth - 1));
    // Measures the current `window` (all channels) as the frame starting at `start`; `new_from`
    // is the index of the first window sample not yet fed to the waveform envelope.
    auto analyze_window = [&](std::uint64_t start, std::size_t new_from) {
        for (std::uint16_t channel = 0; channel < metadata.channels; ++channel) {
            for (std::size_t i = 0; i < window_size; ++i)
                samples[i] = window[i * metadata.channels + channel];
            // Overlapping FFT windows revisit samples; Welch receives only the new suffix.
            estimators[channel].push(std::span<const double>(samples).subspan(new_from));
            const auto spectrum = analyze_spectrum(samples, metadata.sample_rate_hz);
            if (row_edges.empty()) {
                // Keep the lowest row below Nyquist for very low sample rates.
                row_edges = log_row_edges(spectrum, settings.spectrogram_rows,
                                          std::min(settings.spectrogram_min_hz, spectrum.back().frequency_hz / 100.0));
                for (auto& view : local_views)
                    view.row_frequency_hz.assign(row_edges.begin(), row_edges.end() - 1);
            }
            local_views[channel].spectrogram_dbfs.push_back(
                reduce_to_rows(spectrum, row_edges, settings.spectrogram_floor_dbfs));
            envelopes[channel].add(samples.data() + new_from, window_size - new_from);
            for (std::size_t i = new_from; i < window_size; ++i)
                if (std::abs(samples[i]) >= clip_threshold) ++report.channel_results[channel].clipped_samples;
            report.channel_results[channel].frames.push_back(
                measure_frame(spectrum, start, report.support_threshold_dbfs, 10, report.minimum_prominence_db));
            FramePeaks entry{start, {}};
            for (const auto& peak : find_peaks(spectrum, settings.peak_min_level_dbfs,
                                               settings.peak_min_prominence_db, settings.max_peaks_per_frame))
                entry.peaks.push_back({peak.frequency_hz, peak.level_dbfs});
            peaks[channel].push_back(std::move(entry));
        }
    };

    std::uint64_t window_start = 0;
    std::size_t new_from = 0; // the first window contributes all samples, later ones their new hop
    while (true) {
        if (cancelled && cancelled())
            throw AnalysisCancelled();
        analyze_window(window_start, new_from);
        if (progress)
            progress(std::min(1.0, static_cast<double>(window_start + window_size) / static_cast<double>(frames)));
        if (window_start + hop_size + window_size > frames)
            break;
        const auto retained = hop_size * metadata.channels;
        std::move(window.begin() + static_cast<std::ptrdiff_t>(retained), window.end(), window.begin());
        auto next = reader.read_frames(hop_size);
        if (next.size() != retained)
            throw std::runtime_error("WAVE data ended before the declared frame count");
        std::copy(next.begin(), next.end(), window.end() - static_cast<std::ptrdiff_t>(next.size()));
        window_start += hop_size;
        new_from = window_size - hop_size;
    }
    // Final partial hop: fewer than hop_size frames remain. Analyze one extra window aligned to the
    // end of the recording so no audio goes unmeasured (it overlaps the previous window by more
    // than 50%); the remaining samples also feed the waveform envelope.
    const std::uint64_t remaining = frames - (window_start + window_size);
    if (remaining > 0) {
        if (cancelled && cancelled())
            throw AnalysisCancelled();
        const auto tail = static_cast<std::size_t>(remaining);
        auto next = reader.read_frames(tail);
        if (next.size() != tail * metadata.channels)
            throw std::runtime_error("WAVE data ended before the declared frame count");
        const auto shift = tail * metadata.channels;
        std::move(window.begin() + static_cast<std::ptrdiff_t>(shift), window.end(), window.begin());
        std::copy(next.begin(), next.end(), window.end() - static_cast<std::ptrdiff_t>(shift));
        window_start = frames - window_size;
        analyze_window(window_start, window_size - tail);
        if (progress)
            progress(1.0);
    }

    auto tracker = settings.tracker;
    tracker.bin_spacing_hz = static_cast<double>(metadata.sample_rate_hz) / static_cast<double>(window_size);
    report.tracker = tracker;
    for (std::size_t c = 0; c < peaks.size(); ++c)
        report.channel_results[c].tracks = track_peaks(peaks[c], metadata.sample_rate_hz, tracker,
                                                       &report.channel_results[c].suppressed_tracks);
    PsdNoiseFloorSettings floor_settings;
    floor_settings.minimum_prominence_db = settings.minimum_prominence_db;
    for (std::size_t c = 0; c < estimators.size(); ++c)
        report.channel_results[c].psd = summarize_welch_psd(estimators[c].finish(), floor_settings);
    for (const auto& channel : report.channel_results) {
        if (channel.clipped_samples >= 10)
            report.warnings.push_back("channel " + std::to_string(channel.channel_number) + ": " +
                std::to_string(channel.clipped_samples) + " samples at or beyond full scale (possible clipping; levels near 0 dBFS are unreliable)");
        if (channel.suppressed_tracks > 0)
            report.warnings.push_back("channel " + std::to_string(channel.channel_number) + ": " +
                std::to_string(channel.suppressed_tracks) + " track(s) seen in fewer than " +
                std::to_string(tracker.min_observed_frames) + " frames are not listed");
    }
    for (std::size_t c = 0; c < local_views.size(); ++c)
    {
        envelopes[c].finish(local_views[c].waveform_min, local_views[c].waveform_max);
        local_views[c].waveform_samples_per_bucket = envelopes[c].samples_per_bucket();
    }
    if (views)
        *views = std::move(local_views);
    return report;
}

} // namespace humtrace
