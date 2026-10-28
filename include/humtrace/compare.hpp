// Purpose: reproducible multi-file comparison of mains-harmonic measurements.
// Summarizes measurements per channel; makes no source or identity claim.
#pragma once

#include "humtrace/report.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace humtrace {

struct ChannelSummary {
    std::uint16_t channel_number{};
    std::size_t frames{};
    // Frames where at least `min_support` nominal harmonics met both thresholds.
    std::size_t frames_50_hz_supported{};
    std::size_t frames_60_hz_supported{};
    double median_peak_hz{};
    double median_peak_dbfs{};
};

struct FileSummary {
    std::string path;
    std::string sha256;
    std::vector<ChannelSummary> channels;
};

[[nodiscard]] FileSummary summarize(const AnalysisReport& report, std::size_t min_support = 3);

// Plain-text side-by-side table. Deterministic for identical inputs.
[[nodiscard]] std::string format_comparison(const std::vector<FileSummary>& files);

} // namespace humtrace
