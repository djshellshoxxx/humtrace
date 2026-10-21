#pragma once

#include "humtrace/spectrum.hpp"
#include "humtrace/tracks.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace humtrace {

struct ReportChannel {
    std::uint16_t channel_number{};
    std::vector<InterferenceFrame> frames;
    std::vector<PeakTrack> tracks;
};

struct AnalysisReport {
    std::string input_path;
    std::uint32_t sample_rate_hz{};
    std::uint16_t channels{};
    std::uint16_t bit_depth{};
    std::uint16_t format_code{};
    double duration_seconds{};
    std::size_t frame_size_samples{};
    std::size_t hop_size_samples{};
    double support_threshold_dbfs{-60.0};
    double minimum_prominence_db{10.0};
    // Shared with support_threshold_dbfs and minimum_prominence_db above.
    PeakPickingSettings peak_picking;
    TrackSettings track_settings;
    std::vector<ReportChannel> channel_results;
};

[[nodiscard]] std::string serialize_report_json(const AnalysisReport& report);

} // namespace humtrace
