#pragma once

#include "humtrace/spectrum.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace humtrace {

// A local spectral maximum with interpolated frequency/level and its height
// above the local spectral floor.
struct LocalPeak {
    double frequency_hz{};
    double level_dbfs{};
    double prominence_db{};
};

struct PeakSettings {
    std::size_t max_peaks{8};
    double minimum_level_dbfs{-80.0};
    double minimum_prominence_db{12.0};
};

// Find up to max_peaks non-DC local maxima, strongest first (ties: lower frequency).
[[nodiscard]] std::vector<LocalPeak> find_local_peaks(
    const std::vector<SpectrumBin>& spectrum, const PeakSettings& settings = {});

struct TrackerSettings {
    double fixed_hz_tolerance{0.5};
    double relative_tolerance{0.005};
    double bin_fraction_tolerance{1.5};
    std::size_t max_gap_frames{1};
    std::size_t min_observations{3};
};

struct TrackObservation {
    std::uint64_t start_sample{};
    double frequency_hz{};
    double level_dbfs{};
    double prominence_db{};
};

struct Track {
    std::size_t id{};  // local to one analysis; may change when settings change
    std::vector<TrackObservation> observations;
    std::size_t missing_frames{};  // explicit gaps; never interpolated
    std::uint64_t first_sample{};
    std::uint64_t last_sample{};
    double median_frequency_hz{};
    double median_level_dbfs{};
    double drift_hz_per_second{};  // least-squares slope; 0 when < 2 observations
    std::string end_reason;        // "gap" or "end_of_input"
};

// Deterministic greedy frame-to-frame association within one channel.
// Frequency distance dominates; ties break on track id then peak order.
class PeakTracker {
public:
    PeakTracker(const TrackerSettings& settings, double bin_spacing_hz, double sample_rate_hz);
    void add_frame(std::uint64_t start_sample, const std::vector<LocalPeak>& peaks);
    // Finalize. Tracks shorter than min_observations are dropped. Not reusable.
    [[nodiscard]] std::vector<Track> finish();

private:
    struct Active { Track track; std::size_t trailing_missing{}; };
    void close(Active& active, const char* reason);
    TrackerSettings settings_;
    double bin_spacing_hz_;
    double sample_rate_hz_;
    std::size_t next_id_{1};
    std::vector<Active> active_;
    std::vector<Track> finished_;
};

} // namespace humtrace
