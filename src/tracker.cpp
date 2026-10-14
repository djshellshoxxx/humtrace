#include "humtrace/tracker.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace humtrace {
namespace {
[[nodiscard]] double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 != 0 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

void summarize(Track& track, double sample_rate_hz) {
    const auto& obs = track.observations;
    track.first_frame = obs.front().frame_index;
    track.last_frame = obs.back().frame_index;
    track.observed_frames = obs.size();
    track.missing_frames = track.last_frame - track.first_frame + 1 - obs.size();
    std::vector<double> freqs, levels;
    for (const auto& o : obs) {
        freqs.push_back(o.frequency_hz);
        levels.push_back(o.level_dbfs);
    }
    track.median_frequency_hz = median(freqs);
    track.median_level_dbfs = median(levels);
    track.drift_hz_per_second = 0.0;
    if (obs.size() < 2)
        return;
    const auto n = static_cast<double>(obs.size());
    double sum_t = 0.0, sum_f = 0.0;
    for (const auto& o : obs) {
        sum_t += static_cast<double>(o.start_sample) / sample_rate_hz;
        sum_f += o.frequency_hz;
    }
    const double mean_t = sum_t / n, mean_f = sum_f / n;
    double covariance = 0.0, variance = 0.0;
    for (const auto& o : obs) {
        const double dt = static_cast<double>(o.start_sample) / sample_rate_hz - mean_t;
        covariance += dt * (o.frequency_hz - mean_f);
        variance += dt * dt;
    }
    if (variance > 0.0)
        track.drift_hz_per_second = covariance / variance;
}
} // namespace

std::vector<Track> track_peaks(const std::vector<FramePeaks>& frames, double sample_rate_hz,
                               const TrackerSettings& settings) {
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0 ||
        !std::isfinite(settings.fixed_hz_tolerance) || settings.fixed_hz_tolerance < 0.0 ||
        !std::isfinite(settings.relative_tolerance) || settings.relative_tolerance < 0.0 ||
        !std::isfinite(settings.bin_spacing_hz) || settings.bin_spacing_hz < 0.0 ||
        !std::isfinite(settings.bin_fraction) || settings.bin_fraction < 0.0)
        throw std::invalid_argument("invalid tracker settings or sample rate");

    std::vector<Track> tracks;
    std::vector<std::size_t> active; // indices into tracks, ascending id
    for (std::size_t frame = 0; frame < frames.size(); ++frame) {
        std::erase_if(active, [&](std::size_t t) {
            return frame - tracks[t].observations.back().frame_index > settings.max_gap_frames + 1;
        });
        const auto& peaks = frames[frame].peaks;
        std::vector<std::tuple<double, std::size_t, std::size_t>> candidates; // distance, track, peak
        for (std::size_t p = 0; p < peaks.size(); ++p) {
            if (!std::isfinite(peaks[p].frequency_hz) || peaks[p].frequency_hz <= 0.0 ||
                !std::isfinite(peaks[p].level_dbfs))
                continue;
            for (const auto t : active) {
                const double prior = tracks[t].observations.back().frequency_hz;
                const double limit = std::max({settings.fixed_hz_tolerance,
                                               settings.relative_tolerance * prior,
                                               settings.bin_fraction * settings.bin_spacing_hz});
                const double distance = std::abs(peaks[p].frequency_hz - prior);
                if (distance <= limit)
                    candidates.emplace_back(distance, t, p);
            }
        }
        std::sort(candidates.begin(), candidates.end());
        std::vector<bool> track_used(tracks.size(), false), peak_used(peaks.size(), false);
        for (const auto& [distance, t, p] : candidates) {
            if (track_used[t] || peak_used[p])
                continue;
            track_used[t] = peak_used[p] = true;
            tracks[t].observations.push_back(
                {frame, frames[frame].start_sample, peaks[p].frequency_hz, peaks[p].level_dbfs});
        }
        for (std::size_t p = 0; p < peaks.size(); ++p) {
            if (peak_used[p] || !std::isfinite(peaks[p].frequency_hz) ||
                peaks[p].frequency_hz <= 0.0 || !std::isfinite(peaks[p].level_dbfs))
                continue;
            Track track;
            track.id = tracks.size() + 1;
            track.observations.push_back(
                {frame, frames[frame].start_sample, peaks[p].frequency_hz, peaks[p].level_dbfs});
            tracks.push_back(std::move(track));
            active.push_back(tracks.size() - 1);
        }
    }
    for (auto& track : tracks)
        summarize(track, sample_rate_hz);
    return tracks;
}

} // namespace humtrace
