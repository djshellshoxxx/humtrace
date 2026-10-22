#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace humtrace {

struct TrackPeak {
    double frequency_hz{};
    double level_dbfs{};
};

struct FramePeaks {
    std::uint64_t start_sample{};
    std::vector<TrackPeak> peaks;
};

struct TrackerSettings {
    double fixed_hz_tolerance{1.0};
    double relative_tolerance{0.0};
    double bin_spacing_hz{0.0};
    double bin_fraction{1.0};
    std::size_t max_gap_frames{1};
    // Tracks observed in fewer frames are not reported (raw per-frame measurements are kept
    // elsewhere). Suppresses single-frame noise flicker; not a validated persistence rule.
    std::size_t min_observed_frames{3};
};

struct TrackObservation {
    std::size_t frame_index{};
    std::uint64_t start_sample{};
    double frequency_hz{};
    double level_dbfs{};
};

struct Track {
    std::size_t id{}; // local to one analysis; not stable across settings
    std::vector<TrackObservation> observations;
    std::size_t first_frame{};
    std::size_t last_frame{};
    std::size_t observed_frames{};
    std::size_t missing_frames{}; // explicit gaps; never interpolated
    double median_frequency_hz{};
    double median_level_dbfs{};
    double drift_hz_per_second{};
};

// Associate peaks across ordered frames of ONE channel. Deterministic: greedy
// one-to-one matching by (distance, track id, peak index). Throws
// std::invalid_argument on invalid settings or sample rate. If `suppressed` is given it
// receives the number of tracks dropped for having fewer than min_observed_frames frames.
[[nodiscard]] std::vector<Track> track_peaks(const std::vector<FramePeaks>& frames,
                                             double sample_rate_hz,
                                             const TrackerSettings& settings,
                                             std::size_t* suppressed = nullptr);

} // namespace humtrace
