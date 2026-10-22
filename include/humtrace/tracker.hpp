#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
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

// Incremental deterministic association. Storage of observations is delegated to
// callbacks; only active track endpoints and bounded candidate pairs are retained.
class PeakAssociator {
public:
    using Observe = std::function<void(std::size_t, const TrackObservation&)>;
    using End = std::function<void(std::size_t)>;
    explicit PeakAssociator(TrackerSettings settings, std::size_t max_active = 4096);
    void push(const FramePeaks& frame, const Observe& observed, const End& ended);
    void finish(const End& ended);
    [[nodiscard]] std::size_t active_count() const { return active_.size(); }
    [[nodiscard]] std::size_t tracks_created() const { return next_id_ - 1; }
private:
    struct Active { std::size_t id; TrackObservation last; };
    TrackerSettings settings_;
    std::size_t max_active_;
    std::size_t frame_index_{};
    std::size_t next_id_{1};
    std::vector<Active> active_;
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
