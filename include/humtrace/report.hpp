#pragma once

#include "humtrace/spectrum.hpp"
#include "humtrace/psd.hpp"
#include "humtrace/tracker.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace humtrace {

struct ReportChannel {
    std::uint16_t channel_number{};
    std::vector<InterferenceFrame> frames;
    std::optional<PsdSummary> psd;
    std::vector<Track> tracks; // dominant-peak persistence; descriptive only
    std::size_t suppressed_tracks{};  // tracks below min_observed_frames, not listed
    std::uint64_t clipped_samples{};  // samples at or beyond full scale (analyzed region)
    std::vector<TrackAmbiguity> ambiguities; // channel-local links, including suppressed segments
};

struct AnalysisReport {
    std::string input_path;
    std::string input_sha256; // lowercase hex of the exact input bytes; empty if not computed
    std::uint32_t sample_rate_hz{};
    std::uint16_t channels{};
    std::uint16_t bit_depth{};
    std::uint16_t format_code{};
    double duration_seconds{};
    std::size_t frame_size_samples{};
    std::size_t hop_size_samples{};
    double support_threshold_dbfs{-60.0};
    double minimum_prominence_db{10.0};
    TrackerSettings tracker;
    WelchSettings welch;
    // Remaining pipeline settings that change results (serialized so a report is reproducible).
    double peak_min_level_dbfs{-80.0};
    double peak_min_prominence_db{10.0};
    std::size_t max_peaks_per_frame{8};
    std::size_t max_harmonics{10};
    std::uint64_t input_bytes{};      // exact byte length of the input file
    std::vector<std::string> warnings; // plain-language notes about measurement quality
    std::vector<ReportChannel> channel_results;
    bool spectrum_remove_mean{true};
    PeakSearchSettings peak_search{};
    double harmonic_tolerance_hz{};
};

// Fixed-size effective settings only; suitable for comparison provenance without
// copying source frames, observations, warnings, or ambiguity tables.
struct AnalysisSettingsSnapshot {
    std::size_t frame_size_samples{}, hop_size_samples{};
    double support_threshold_dbfs{}, minimum_prominence_db{};
    TrackerSettings tracker;
    WelchSettings welch;
    double peak_min_level_dbfs{}, peak_min_prominence_db{};
    std::size_t max_peaks_per_frame{}, max_harmonics{};
    bool spectrum_remove_mean{};
    PeakSearchSettings peak_search;
    double harmonic_tolerance_hz{};
    explicit AnalysisSettingsSnapshot(const AnalysisReport& report);
};
void write_analysis_settings_json(std::ostream& output, const AnalysisSettingsSnapshot& settings);

// Array providers allow reports to be assembled from bounded-memory disk spools.
struct ReportArrays {
    std::function<void(std::ostream&, std::size_t)> frames;
    std::function<void(std::ostream&, std::size_t)> tracks;
    std::function<void(std::ostream&, std::size_t)> ambiguities;
};
void write_frame_json(std::ostream& output, const InterferenceFrame& frame, std::uint32_t sample_rate_hz);
void write_track_json(std::ostream& output, const Track& track,
                      const std::function<void(std::ostream&)>& observations = {},
                      const std::function<void(std::ostream&)>& gaps = {});
void write_ambiguity_json(std::ostream& output, const TrackAmbiguity& event);
void write_report_json(std::ostream& output, const AnalysisReport& report, const ReportArrays* arrays = nullptr);

[[nodiscard]] std::string serialize_report_json(const AnalysisReport& report);

} // namespace humtrace
