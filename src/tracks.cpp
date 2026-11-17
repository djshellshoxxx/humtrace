#include "humtrace/tracks.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace humtrace {
namespace {
struct OpenTrack {
    std::size_t first_frame{};
    std::size_t last_frame{};
    double last_frequency_hz{};
    std::vector<TrackObservation> observations;
};

struct Candidate {
    double distance_hz;
    std::size_t track;
    std::size_t peak;
};
} // namespace

std::vector<PeakTrack> associate_peak_tracks(const std::vector<PeakFrame>& frames,
                                             double sample_rate_hz,
                                             const TrackSettings& settings,
                                             std::uint16_t channel_number) {
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0 ||
        !std::isfinite(settings.frequency_tolerance_hz) || settings.frequency_tolerance_hz < 0.0 ||
        settings.min_observed_frames == 0)
        throw std::invalid_argument("invalid track settings");
    for (std::size_t frame = 1; frame < frames.size(); ++frame)
        if (frames[frame].start_sample <= frames[frame - 1].start_sample)
            throw std::invalid_argument("frames must be ordered by increasing start sample");

    // Creation order is the deterministic tie-break and the eventual track number order.
    std::vector<OpenTrack> tracks;
    std::vector<std::size_t> active;
    std::vector<Candidate> candidates;
    std::vector<bool> track_matched;
    std::vector<bool> peak_matched;

    for (std::size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        const auto& frame = frames[frame_index];
        const auto& peaks = frame.peaks;
        candidates.clear();
        track_matched.assign(tracks.size(), false);
        peak_matched.assign(peaks.size(), false);
        for (const auto track : active) {
            for (std::size_t peak = 0; peak < peaks.size(); ++peak) {
                const double distance = std::abs(peaks[peak].frequency_hz -
                                                 tracks[track].last_frequency_hz);
                if (distance <= settings.frequency_tolerance_hz)
                    candidates.push_back({distance, track, peak});
            }
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.distance_hz != rhs.distance_hz) return lhs.distance_hz < rhs.distance_hz;
            if (lhs.track != rhs.track) return lhs.track < rhs.track;
            return lhs.peak < rhs.peak;
        });
        for (const auto& candidate : candidates) {
            if (track_matched[candidate.track] || peak_matched[candidate.peak])
                continue;
            track_matched[candidate.track] = true;
            peak_matched[candidate.peak] = true;
            auto& track = tracks[candidate.track];
            const auto& peak = peaks[candidate.peak];
            track.last_frame = frame_index;
            track.last_frequency_hz = peak.frequency_hz;
            track.observations.push_back({frame.start_sample, peak.frequency_hz,
                                          peak.level_dbfs, peak.prominence_db});
        }
        const auto first_new_track = tracks.size();
        for (std::size_t peak = 0; peak < peaks.size(); ++peak) {
            if (peak_matched[peak])
                continue;
            tracks.push_back({frame_index, frame_index, peaks[peak].frequency_hz,
                              {{frame.start_sample, peaks[peak].frequency_hz,
                                peaks[peak].level_dbfs, peaks[peak].prominence_db}}});
        }

        // Keep tracks that may still match the next frame within the allowed gap.
        // Indices stay in creation order, which keeps the matching deterministic.
        std::vector<std::size_t> next_active;
        for (const auto track : active)
            if (frame_index - tracks[track].last_frame <= settings.max_gap_frames)
                next_active.push_back(track);
        for (auto track = first_new_track; track < tracks.size(); ++track)
            next_active.push_back(track);
        active = std::move(next_active);
    }

    std::vector<PeakTrack> result;
    std::size_t track_number = 0;
    for (const auto& track : tracks) {
        const auto& observations = track.observations;
        if (observations.size() < settings.min_observed_frames)
            continue;
        PeakTrack summary;
        summary.channel_number = channel_number;
        summary.track_number = ++track_number;
        summary.start_sample = observations.front().start_sample;
        summary.end_sample = observations.back().start_sample;
        summary.missing_frames = (track.last_frame - track.first_frame + 1) - observations.size();
        summary.min_frequency_hz = std::numeric_limits<double>::infinity();
        summary.max_frequency_hz = -std::numeric_limits<double>::infinity();
        summary.max_level_dbfs = -std::numeric_limits<double>::infinity();
        double frequency_sum = 0.0;
        double level_sum = 0.0;
        for (const auto& observation : observations) {
            frequency_sum += observation.frequency_hz;
            level_sum += observation.level_dbfs;
            summary.min_frequency_hz = std::min(summary.min_frequency_hz, observation.frequency_hz);
            summary.max_frequency_hz = std::max(summary.max_frequency_hz, observation.frequency_hz);
            summary.max_level_dbfs = std::max(summary.max_level_dbfs, observation.level_dbfs);
        }
        const auto count = static_cast<double>(observations.size());
        summary.mean_frequency_hz = frequency_sum / count;
        summary.mean_level_dbfs = level_sum / count;
        summary.frequency_drift_hz = observations.back().frequency_hz - observations.front().frequency_hz;
        const auto elapsed_samples = summary.end_sample - summary.start_sample;
        summary.drift_rate_hz_per_second =
            elapsed_samples == 0 ? std::numeric_limits<double>::quiet_NaN()
                                 : summary.frequency_drift_hz * sample_rate_hz /
                                       static_cast<double>(elapsed_samples);
        summary.observations = observations;
        result.push_back(std::move(summary));
    }
    return result;
}

} // namespace humtrace
