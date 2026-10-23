#pragma once

#include "humtrace/session.hpp"
#include <memory>

namespace humtrace {

struct StreamingLimits {
    std::uint64_t max_disk_bytes{4ULL * 1024 * 1024 * 1024};
    std::uint64_t min_free_disk_bytes{16ULL * 1024 * 1024};
    std::uint64_t max_resident_bytes{512ULL * 1024 * 1024};
    std::size_t max_active_tracks{4096};
};

struct FrameEvidence {
    std::size_t lo50{1000}, hi50{}, lo60{1000}, hi60{}, harmonic_count{};
    bool measured{}, below_floor{};
    double rms_lo{}, rms_hi{}, sample_peak{sample_level_floor_dbfs};
    std::size_t harmonic_count_60{};
};
struct StoredTrack {
    std::size_t id{}, first_frame{}, last_frame{}, observed_frames{}, missing_frames{};
    double median_frequency_hz{}, median_level_dbfs{}, drift_hz_per_second{};
    FrameEvidence evidence;
    std::optional<std::size_t> start_ambiguity_id{}, end_ambiguity_id{};
    TrackEndReason end_reason{TrackEndReason::EndOfInput};
};

// Immutable private on-disk analysis with bounded random-access reads. Returned
// values are independent snapshots; storage lives until its last shared owner.
class DiskAnalysis {
public:
    struct Impl;
    explicit DiskAnalysis(std::shared_ptr<Impl> impl);
    [[nodiscard]] const AnalysisReport& report() const;
    [[nodiscard]] const std::vector<ChannelView>& views() const;
    [[nodiscard]] std::uint64_t frame_count(std::size_t channel) const;
    [[nodiscard]] std::uint64_t track_count(std::size_t channel) const;
    [[nodiscard]] InterferenceFrame frame(std::size_t channel, std::uint64_t index) const;
    [[nodiscard]] std::string frame_json(std::size_t channel, std::uint64_t frame) const;
    [[nodiscard]] StoredTrack track(std::size_t channel, std::uint64_t index) const;
    [[nodiscard]] std::vector<float> spectrogram_row(std::size_t channel, std::uint64_t index, unsigned level = 0) const;
    [[nodiscard]] std::uint64_t frame_start_sample(std::size_t channel, std::uint64_t frame) const;
    void export_json(const std::string& output, bool replace_existing, bool full_input_path,
                     const SessionCallbacks& callbacks = {}) const;
private:
    std::shared_ptr<Impl> impl_;
};
[[nodiscard]] std::shared_ptr<DiskAnalysis> analyze_file_paged(
    const std::string& input, const PipelineSettings& settings,
    const SessionCallbacks& callbacks = {}, const StreamingLimits& limits = {});

struct StreamingResult {
    AnalysisReport summary; // no retained frames or track observations
    std::vector<std::uint64_t> frame_counts;
};

// Incrementally spool frames and observations on the output filesystem, then
// publish a schema-compatible JSON report atomically. Exact medians use bounded
// disk scans rather than retaining an observation vector. All scratch is removed
// on success, cancellation, quota exhaustion or failure. Input is never modified.
[[nodiscard]] StreamingResult stream_file_report(
    const std::string& input, const std::string& output, const PipelineSettings& settings,
    const SessionCallbacks& callbacks = {}, const StreamingLimits& limits = {},
    bool replace_existing = false, bool full_input_path = false);

} // namespace humtrace
