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
    WelchSettings welch;
    double peak_min_level_dbfs{-80.0};
    double peak_min_prominence_db{10.0};
    std::size_t max_peaks_per_frame{8};
    double support_threshold_dbfs{-60.0};
    double minimum_prominence_db{10.0};
    std::size_t waveform_buckets{2048};
    std::size_t spectrogram_rows{128};
    double spectrogram_min_hz{20.0};
    double spectrogram_floor_dbfs{-120.0};
    // Zero retains automatic selection / half-window hop. Explicit windows are 16..65536,
    // power of two; hops are 1..effective window, with complete sample coverage.
    std::size_t frame_size_samples{0};
    std::size_t hop_size_samples{0};
    bool spectrum_remove_mean{true};
    PeakSearchSettings peak_search;
    // Zero preserves nominal-bin inspection; positive enables bounded local-maximum search.
    double harmonic_tolerance_hz{0.0};
};

// Optional disk-backed consumer; measurements are delivered before scratch reuse.
// With a sink, the returned report contains metadata/PSD and no retained timeline.
class FrameSink {
public:
    virtual ~FrameSink() = default;
    virtual void begin(const WavMetadata&, const PipelineSettings&) {}
    [[nodiscard]] virtual bool wants_views() const { return false; }
    virtual void view_row(std::size_t, const std::vector<float>&) {}
    virtual void envelope(std::size_t, ChannelView&&) {}
    virtual void frame(std::size_t channel, const InterferenceFrame& frame, const FramePeaks& peaks) = 0;
    virtual void finish(std::size_t channel, ReportChannel& summary) = 0;
};

// Throws std::invalid_argument naming the offending setting (non-finite values, negative
// tolerances, zero counts). analyze_wav calls this first.
void validate_settings(const PipelineSettings& settings);

// Choose a power-of-two analysis window for the given metadata. Throws
// std::runtime_error when the audio is too short or the channel count is excessive.
[[nodiscard]] std::size_t select_window_size(const WavMetadata& metadata,
                                             std::size_t max_interleaved_window_samples);
[[nodiscard]] std::size_t select_window_size(const WavMetadata& metadata,
                                             const PipelineSettings& settings);

// Stream a WAV through configured rolling windows, measuring each channel
// independently, then associate each channel's strongest peaks into tracks.
// `progress` (optional) receives the fraction complete in [0, 1]; `cancelled` is polled
// once per window and throws AnalysisCancelled when it returns true.
// When `views` is non-null it is filled with one ChannelView per channel (presentation only).
[[nodiscard]] AnalysisReport analyze_wav(WavStreamReader& reader, const PipelineSettings& settings,
                                         const std::function<void(double)>& progress = {},
                                         const std::function<bool()>& cancelled = {},
                                         std::vector<ChannelView>* views = nullptr, FrameSink* sink = nullptr,
                                         const std::function<void()>& finalizing = {});

struct AnalysisCancelled : std::runtime_error {
    AnalysisCancelled() : std::runtime_error("analysis cancelled") {}
};

} // namespace humtrace
