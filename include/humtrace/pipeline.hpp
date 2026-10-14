#pragma once

#include "humtrace/report.hpp"
#include "humtrace/tracker.hpp"
#include "humtrace/view.hpp"
#include "humtrace/wav.hpp"

#include <cstddef>
#include <functional>
#include <stdexcept>

namespace humtrace {

struct PipelineSettings {
    std::size_t max_interleaved_window_samples{16 * 1024 * 1024};
    // Upper bound on retained results (frames, peaks, spectrogram rows). The run is refused up
    // front, never silently downsampled, when the estimate exceeds it.
    std::uint64_t max_result_bytes{512ULL * 1024 * 1024};
    TrackerSettings tracker;
    double peak_min_level_dbfs{-80.0};
    double peak_min_prominence_db{10.0};
    std::size_t max_peaks_per_frame{8};
    double support_threshold_dbfs{-60.0};
    double minimum_prominence_db{10.0};
    std::size_t waveform_buckets{2048};
    std::size_t spectrogram_rows{128};
    double spectrogram_min_hz{20.0};
    double spectrogram_floor_dbfs{-120.0};
};

// Throws std::invalid_argument naming the offending setting (non-finite values, negative
// tolerances, zero counts). analyze_wav calls this first.
void validate_settings(const PipelineSettings& settings);

// Choose a power-of-two analysis window for the given metadata. Throws
// std::runtime_error when the audio is too short or the channel count is excessive.
[[nodiscard]] std::size_t select_window_size(const WavMetadata& metadata,
                                             std::size_t max_interleaved_window_samples);

// Stream a WAV through rolling windows (hop = window / 2), measuring each channel
// independently, then associate each channel's strongest peaks into tracks.
// `progress` (optional) receives the fraction complete in [0, 1]; `cancelled` is polled
// once per window and throws AnalysisCancelled when it returns true.
// When `views` is non-null it is filled with one ChannelView per channel (presentation only).
[[nodiscard]] AnalysisReport analyze_wav(WavStreamReader& reader, const PipelineSettings& settings,
                                         const std::function<void(double)>& progress = {},
                                         const std::function<bool()>& cancelled = {},
                                         std::vector<ChannelView>* views = nullptr);

struct AnalysisCancelled : std::runtime_error {
    AnalysisCancelled() : std::runtime_error("analysis cancelled") {}
};

} // namespace humtrace
