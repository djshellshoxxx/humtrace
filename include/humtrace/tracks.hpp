#pragma once

#include "humtrace/spectrum.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace humtrace {

struct TrackSettings {
    // Maximum |frequency change| between a track's last observation and a new peak.
    double frequency_tolerance_hz{1.0};
    // Unobserved frames allowed between two observations of the same track.
    std::size_t max_gap_frames{1};
    // Tracks observed in fewer frames are not reported.
    std::size_t min_observed_frames{3};
};

// One analysis frame of one channel. Frames must be ordered by start_sample and
// spaced by a uniform hop; frame positions define the gap counts.
struct PeakFrame {
    std::uint64_t start_sample{};
    std::vector<PeakCandidate> peaks;
};

struct TrackObservation {
    std::uint64_t start_sample{};
    double frequency_hz{};
    double level_dbfs{};
    double prominence_db{};
};

struct PeakTrack {
    std::uint16_t channel_number{};
    // 1-based, in order of first observation within the channel and analysis.
    std::size_t track_number{};
    // Window start samples of the first and last observed frames.
    std::uint64_t start_sample{};
    std::uint64_t end_sample{};
    // Frames inside [start, end] without an observation. Gaps are not interpolated.
    std::size_t missing_frames{};
    double mean_frequency_hz{};
    double min_frequency_hz{};
    double max_frequency_hz{};
    double mean_level_dbfs{};
    double max_level_dbfs{};
    // Last minus first observed frequency, and that change divided by the elapsed
    // time between the first and last observations. Rate is NaN when undefined.
    double frequency_drift_hz{};
    double drift_rate_hz_per_second{};
    std::vector<TrackObservation> observations;
};

// Associate peaks into tracks within one channel using greedy nearest-frequency
// matching. Output is deterministic for identical inputs and settings.
[[nodiscard]] std::vector<PeakTrack> associate_peak_tracks(
    const std::vector<PeakFrame>& frames, double sample_rate_hz,
    const TrackSettings& settings, std::uint16_t channel_number);

} // namespace humtrace
