#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
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

enum class TrackEndReason { EndOfInput, GapExceeded, Ambiguous };

enum class TrackAmbiguityKind { Split, Merge, Competing };

// Candidate-graph relationships, not evidence of a physical source splitting or
// merging. IDs are local to this channel/analysis, including suppressed segments.
struct TrackAmbiguity {
    std::size_t id{};
    std::size_t frame_index{};
    std::uint64_t start_sample{};
    TrackAmbiguityKind kind{TrackAmbiguityKind::Competing};
    std::vector<std::size_t> parent_track_ids;
    std::vector<std::size_t> child_track_ids;
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
    double frequency_q25_hz{}, frequency_q75_hz{}, level_q25_dbfs{}, level_q75_dbfs{};
    std::optional<double> drift_r_squared;
    std::optional<double> drift_residual_rms_hz;
    TrackEndReason end_reason{TrackEndReason::EndOfInput};
    std::optional<std::size_t> start_ambiguity_id;
    std::optional<std::size_t> end_ambiguity_id;
};
// Shared exact reduction for retained and disk-backed observations. The scan may
// be called repeatedly; order_statistic returns the selected ascending rank.
using ObservationVisitor = std::function<void(const TrackObservation&)>;
using ObservationScan = std::function<void(const ObservationVisitor&)>;
using ObservationOrderStatistic = std::function<double(std::size_t, bool)>;
void summarize_track(Track& track, double sample_rate_hz, const ObservationScan& scan,
                     const ObservationOrderStatistic& order_statistic);

// Incremental deterministic association. Storage of observations is delegated to
// callbacks; only bounded active endpoints and one frame's candidate graph are
// retained. Only isolated 1:1 graph components continue an identity. Other
// connected components end all parents and start fresh children.
class PeakAssociator {
public:
    using Observe = std::function<void(std::size_t, const TrackObservation&)>;
    using End = std::function<void(std::size_t, TrackEndReason)>;
    using Event = std::function<void(const TrackAmbiguity&)>;
    explicit PeakAssociator(TrackerSettings settings, std::size_t max_active = 4096);
    // Event callbacks precede every affected end/observation callback. Parent IDs
    // ascend; child IDs follow the input peak order. Event IDs ascend by the
    // smallest parent ID in each ambiguous component.
    void push(const FramePeaks& frame, const Observe& observed, const End& ended, const Event& event = {});
    void finish(const End& ended);
    [[nodiscard]] std::size_t active_count() const { return active_.size(); }
    [[nodiscard]] std::size_t tracks_created() const { return next_id_ - 1; }
private:
    struct Active { std::size_t id; TrackObservation last; };
    TrackerSettings settings_;
    std::size_t max_active_;
    std::size_t frame_index_{};
    std::size_t next_id_{1};
    std::size_t next_ambiguity_id_{1};
    std::vector<Active> active_;
};

// Associate peaks across ordered frames of ONE channel. Deterministic and
// conservative at split/merge/competing candidates; levels never prove identity.
// Throws std::invalid_argument on invalid settings or sample rate. If `suppressed` is given it
// receives the number of tracks dropped for having fewer than min_observed_frames frames.
[[nodiscard]] std::vector<Track> track_peaks(const std::vector<FramePeaks>& frames,
                                             double sample_rate_hz,
                                             const TrackerSettings& settings,
                                             std::size_t* suppressed = nullptr,
                                             std::vector<TrackAmbiguity>* ambiguities = nullptr);

} // namespace humtrace
