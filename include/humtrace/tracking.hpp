// Purpose: associate per-frame tonal peaks into persistent narrowband tone tracks.
// Tracks are measurements (frequency, level, persistence, drift). A track that
// lies near a 50/60 Hz multiple is labelled as such; that is not a source claim.
#pragma once

#include "humtrace/spectrum.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace humtrace {

struct MainsProximity {
    double nominal_hz{};          // 50 or 60; 0 when the track is not near either
    std::size_t harmonic{};       // integer multiple of nominal_hz
    double deviation_hz{};        // median_frequency_hz - harmonic * nominal_hz
};

struct ToneTrack {
    std::uint64_t start_sample{};   // first frame start
    std::uint64_t end_sample{};     // last frame start + frame size (exclusive)
    std::size_t frames_detected{};
    std::size_t frames_spanned{};   // first..last frame inclusive
    double persistence{};           // frames_detected / frames_spanned
    double median_frequency_hz{};
    double min_frequency_hz{};
    double max_frequency_hz{};
    double drift_hz_per_second{};   // least-squares slope of frequency over frame centers
    double median_level_dbfs{};
    double max_level_dbfs{};
    double median_prominence_db{};
    MainsProximity mains;
};

struct TrackerSettings {
    double frequency_tolerance_hz{0.0};  // <= 0: 1.5 FFT bins
    std::size_t max_gap_frames{2};       // missed frames tolerated inside a track
    // Shorter tracks are discarded. Frames overlap 50%, so adjacent frames share
    // samples and noise peaks persist ~2 frames; 5 frames is ~3 independent ones.
    std::size_t min_frames{5};
    // Also discard tracks spanning fewer hops than this duration; limits chance
    // associations of noise peaks when frames are short.
    double min_duration_seconds{0.5};
    std::size_t max_active_tracks{64};   // bounds per-frame work
    std::size_t max_tracks{4096};        // bounds output size
};

// Online tracker for one channel. Frames must be pushed in increasing index order.
class ToneTracker {
public:
    // Throws std::invalid_argument for non-positive rate/frame/hop.
    ToneTracker(const TrackerSettings& settings, double sample_rate_hz,
                std::size_t frame_size, std::size_t hop_size);
    void push(std::size_t frame_index, const std::vector<TonalPeak>& peaks);
    // Closes all open tracks and returns finished tracks sorted by start, then frequency.
    [[nodiscard]] std::vector<ToneTrack> finish();
    // True when a limit dropped peaks or tracks.
    [[nodiscard]] bool truncated() const { return truncated_; }
    [[nodiscard]] double tolerance_hz() const { return tolerance_hz_; }
    [[nodiscard]] std::size_t effective_min_frames() const { return settings_.min_frames; }

private:
    struct Observation { std::size_t frame; double frequency_hz, level_dbfs, prominence_db; };
    struct Open { std::vector<Observation> obs; };
    void close(Open& track);
    double distance(const Open& track, std::size_t frame, double frequency_hz) const;

    TrackerSettings settings_;
    double sample_rate_hz_;
    std::size_t frame_size_;
    std::size_t hop_size_;
    double tolerance_hz_;
    std::vector<Open> active_;
    std::vector<ToneTrack> done_;
    bool truncated_{false};
    bool has_last_frame_{false};
    std::size_t last_frame_{0};
};

// Nearest 50/60 Hz multiple within tolerance_hz, or {0,0,0}. Harmonics 1..max_harmonic.
[[nodiscard]] MainsProximity nearest_mains(double frequency_hz, double tolerance_hz,
                                           std::size_t max_harmonic = 10);

} // namespace humtrace
