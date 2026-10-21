#pragma once

#include "humtrace/psd.hpp"
#include "humtrace/spectrum.hpp"
#include "humtrace/tracks.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace humtrace {

struct ReportChannel {
    std::uint16_t channel_number{};
    std::vector<InterferenceFrame> frames;
    std::vector<PeakTrack> tracks;
    // Absent when the Welch PSD has no complete segment.
    std::optional<PsdSummary> psd;
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
    // Absent when no Welch PSD was computed for this report.
    std::optional<WelchSettings> welch_psd;
    PsdNoiseFloorSettings psd_noise_floor;
};

[[nodiscard]] std::string serialize_report_json(const AnalysisReport& report);

} // namespace humtrace
