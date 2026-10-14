#pragma once

#include "humtrace/report.hpp"
#include "humtrace/tracker.hpp"
#include "humtrace/wav.hpp"

#include <cstddef>
#include <functional>

namespace humtrace {

struct PipelineSettings {
    std::size_t max_interleaved_window_samples{16 * 1024 * 1024};
    TrackerSettings tracker;
};

// Choose a power-of-two analysis window for the given metadata. Throws
// std::runtime_error when the audio is too short or the channel count is excessive.
[[nodiscard]] std::size_t select_window_size(const WavMetadata& metadata,
                                             std::size_t max_interleaved_window_samples);

// Stream a WAV through rolling windows (hop = window / 2), measuring each channel
// independently, then associate each channel's strongest peaks into tracks.
// `progress` (optional) receives the fraction complete in [0, 1].
[[nodiscard]] AnalysisReport analyze_wav(WavStreamReader& reader, const PipelineSettings& settings,
                                         const std::function<void(double)>& progress = {});

} // namespace humtrace
