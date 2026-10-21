#pragma once

#include "humtrace/compare.hpp"
#include "humtrace/spectrum.hpp"
#include "humtrace/tracker.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace humtrace {

struct ReportChannel {
    std::uint16_t channel_number{};
    std::vector<InterferenceFrame> frames;
    std::vector<Track> tracks;
};

// Explicit, recorded comparison of two recordings. Channels are paired by index
// only; no resampling, alignment, or downmixing is applied.
struct ChannelComparison {
    std::uint16_t channel_number{};
    TrackComparison comparison;
};

struct ReportComparison {
    std::string reference_name;
    std::string reference_sha256;
    std::uint64_t reference_size_bytes{};
    std::uint32_t reference_sample_rate_hz{};
    std::vector<ChannelComparison> channels;
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
    std::uint64_t input_size_bytes{};
    std::string input_sha256;
    bool input_unchanged_during_analysis{true};
    PeakSettings peak_settings;
    TrackerSettings tracker_settings;
    double a4_reference_hz{440.0};
    std::vector<ReportChannel> channel_results;
    std::optional<ReportComparison> comparison;
};

[[nodiscard]] std::string serialize_report_json(const AnalysisReport& report);

} // namespace humtrace
