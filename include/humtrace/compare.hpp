#pragma once

#include "humtrace/tracker.hpp"

#include <cstddef>
#include <vector>

namespace humtrace {

struct TrackMatch {
    std::size_t track_a_id{};
    std::size_t track_b_id{};
    double delta_frequency_hz{};   // b - a, using median frequency
    double delta_level_db{};       // b - a, using median level
    double delta_cents{};          // musical size of the frequency difference
};

struct TrackComparison {
    double frequency_tolerance_hz{};
    std::vector<TrackMatch> matches;
    std::vector<std::size_t> unmatched_a;  // reported as unmatched, never scored as zero
    std::vector<std::size_t> unmatched_b;
};

// Independent-summary comparison (no time alignment, no resampling): pair tracks
// of two recordings whose median frequencies are within tolerance, one-to-one,
// nearest first. Similarity is not evidence of a shared source or location.
[[nodiscard]] TrackComparison compare_tracks(const std::vector<Track>& a, const std::vector<Track>& b,
                                             double frequency_tolerance_hz);

} // namespace humtrace
