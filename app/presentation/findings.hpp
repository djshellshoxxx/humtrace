#pragma once

#include "humtrace/report.hpp"

#include <string>
#include <vector>

namespace humtrace { struct FrameEvidence; }

namespace humtrace::ui {

// One reviewable row per measured track. Descriptive only: no source attribution.
struct FindingRow {
    std::uint16_t channel{};
    std::size_t track_id{};
    double start_seconds{};
    double end_seconds{};
    double median_frequency_hz{};
    double median_level_dbfs{};
    double drift_hz_per_second{};
    std::size_t observed_frames{};
    std::size_t spanned_frames{};
    std::size_t first_frame{};
    std::size_t last_frame{};

    [[nodiscard]] double duration_seconds() const { return end_seconds - start_seconds; }
    [[nodiscard]] double coverage() const {
        return spanned_frames == 0 ? 0.0 : static_cast<double>(observed_frames) / static_cast<double>(spanned_frames);
    }
};

// Ordered by channel, then start time, then analysis-local id (never by a hidden score).
[[nodiscard]] std::vector<FindingRow> build_findings(const AnalysisReport& report);

// Plain-text inspector content: measurements, evidence, settings, limitations.
[[nodiscard]] std::string describe_finding(const AnalysisReport& report, const FindingRow& row,
                                            const FrameEvidence* evidence = nullptr);

[[nodiscard]] std::string summarize_report(const AnalysisReport& report, std::size_t track_count);

// Accessible one-paragraph summary of a whole analysis.
[[nodiscard]] std::string summarize_report(const AnalysisReport& report,
                                           const std::vector<FindingRow>& rows);

} // namespace humtrace::ui
