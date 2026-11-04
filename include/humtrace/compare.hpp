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
    std::vector<ToneTrack> tracks;   // copied from the report
};

struct FileSummary {
    std::string path;
    std::string sha256;
    std::uint32_t sample_rate_hz{};
    std::size_t frame_size_samples{};
    double track_tolerance_hz{};
    std::vector<ChannelSummary> channels;
};

// A tone frequency observed as a track in two or more files (any channel).
struct SharedTone {
    double frequency_hz{};               // median of member track frequencies
    std::vector<std::size_t> files;      // ascending indices into the file list
    MainsProximity mains;
};

// Clusters track median frequencies across files; tolerance is the largest file
// tolerance. Only tracks with persistence >= min_persistence are used.
[[nodiscard]] std::vector<SharedTone> find_shared_tones(const std::vector<FileSummary>& files,
                                                        double min_persistence = 0.5);

[[nodiscard]] FileSummary summarize(const AnalysisReport& report, std::size_t min_support = 3);

// Plain-text side-by-side table. Deterministic for identical inputs.
[[nodiscard]] std::string format_comparison(const std::vector<FileSummary>& files);

// Versioned JSON comparison export ("comparison_schema_version": "1.0").
[[nodiscard]] std::string serialize_comparison_json(const std::vector<FileSummary>& files);

} // namespace humtrace
