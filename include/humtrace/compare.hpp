#pragma once

#include "humtrace/report.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace humtrace {

struct CompareSettings {
    double frequency_tolerance_hz{1.0};
    // Explicit (channel in A, channel in B) pairs, 1-based. Empty = identity mapping over
    // the channels both recordings have. Channels are never mixed or inferred.
    std::vector<std::pair<std::uint16_t, std::uint16_t>> channel_map;
};

struct ComparisonInput {
    std::string path;
    std::string sha256;
    std::uint32_t sample_rate_hz{};
    std::uint16_t channels{};
    double duration_seconds{};
};

struct TrackRef {
    std::uint16_t channel{};
    std::size_t local_id{};
    double median_frequency_hz{};
    double median_level_dbfs{};
};

struct TrackMatch {
    TrackRef a, b;
    double delta_frequency_hz{}; // b - a
    double delta_level_db{};     // b - a
};

struct ComparisonReport {
    ComparisonInput a, b;
    CompareSettings settings;
    std::vector<TrackMatch> matches;
    std::vector<TrackRef> unmatched_a; // explicit: absence is not a zero-valued measurement
    std::vector<TrackRef> unmatched_b;
    std::vector<std::string> warnings;
};

// Independent-summary comparison (no time alignment, no resampling). Pure function of
// two analysis reports. Throws std::invalid_argument for an invalid tolerance, an unknown
// channel, or a map that uses a channel more than once.
[[nodiscard]] ComparisonReport compare_reports(const AnalysisReport& a, const AnalysisReport& b,
                                               const CompareSettings& settings = {});

[[nodiscard]] std::string serialize_comparison_json(const ComparisonReport& report);

} // namespace humtrace
