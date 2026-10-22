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

PeakAssociator::PeakAssociator(TrackerSettings settings, std::size_t max_active)
    : settings_(settings), max_active_(max_active) {
    if (max_active == 0 || max_active > 65536 || !std::isfinite(settings.fixed_hz_tolerance) || settings.fixed_hz_tolerance < 0.0 ||
        !std::isfinite(settings.relative_tolerance) || settings.relative_tolerance < 0.0 ||
        !std::isfinite(settings.bin_spacing_hz) || settings.bin_spacing_hz < 0.0 ||
        !std::isfinite(settings.bin_fraction) || settings.bin_fraction < 0.0)
        throw std::invalid_argument("invalid incremental tracker settings");
}

void PeakAssociator::push(const FramePeaks& frame, const Observe& observed, const End& ended) {
    std::erase_if(active_, [&](const Active& track) {
        if (frame_index_ - track.last.frame_index - 1 > settings_.max_gap_frames) {
            ended(track.id);
            return true;
        }
        return false;
    });
    const auto& peaks = frame.peaks;
    std::vector<std::tuple<double, std::size_t, std::size_t>> candidates;
    constexpr auto candidate_limit = std::size_t{1} << 20;
    const auto peak_count = std::min(peaks.size(), candidate_limit);
    const auto pairs = peak_count != 0 && active_.size() > candidate_limit / peak_count
        ? candidate_limit : active_.size() * peak_count;
    candidates.reserve(pairs);
    for (std::size_t p = 0; p < peaks.size(); ++p) {
        if (!std::isfinite(peaks[p].frequency_hz) || peaks[p].frequency_hz <= 0.0 ||
            !std::isfinite(peaks[p].level_dbfs)) continue;
        for (std::size_t t = 0; t < active_.size(); ++t) {
            const double prior = active_[t].last.frequency_hz;
            const double limit = std::max({settings_.fixed_hz_tolerance,
                settings_.relative_tolerance * prior, settings_.bin_fraction * settings_.bin_spacing_hz});
            const double distance = std::abs(peaks[p].frequency_hz - prior);
            if (distance <= limit) {
                if (candidates.size() >= (std::size_t{1} << 20))
                    throw std::runtime_error("tracker candidate memory limit exceeded");
                candidates.emplace_back(distance, t, p);
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());
    std::vector<bool> track_used(active_.size(), false), peak_used(peaks.size(), false);
    for (const auto& [distance, t, p] : candidates) {
        (void)distance;
        if (track_used[t] || peak_used[p]) continue;
        track_used[t] = peak_used[p] = true;
        active_[t].last = {frame_index_, frame.start_sample, peaks[p].frequency_hz, peaks[p].level_dbfs};
        observed(active_[t].id, active_[t].last);
    }
    for (std::size_t p = 0; p < peaks.size(); ++p) {
        if (peak_used[p] || !std::isfinite(peaks[p].frequency_hz) || peaks[p].frequency_hz <= 0.0 ||
            !std::isfinite(peaks[p].level_dbfs)) continue;
        if (active_.size() >= max_active_) throw std::runtime_error("active tracker memory limit exceeded");
        Active track{next_id_++, {frame_index_, frame.start_sample, peaks[p].frequency_hz, peaks[p].level_dbfs}};
        observed(track.id, track.last);
        active_.push_back(track);
    }
    ++frame_index_;
}

void PeakAssociator::finish(const End& ended) {
    for (const auto& track : active_) ended(track.id);
    active_.clear();
}

std::vector<Track> track_peaks(const std::vector<FramePeaks>& frames, double sample_rate_hz,
                               const TrackerSettings& settings, std::size_t* suppressed) {
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0 ||
        !std::isfinite(settings.fixed_hz_tolerance) || settings.fixed_hz_tolerance < 0.0 ||
        !std::isfinite(settings.relative_tolerance) || settings.relative_tolerance < 0.0 ||
        !std::isfinite(settings.bin_spacing_hz) || settings.bin_spacing_hz < 0.0 ||
        !std::isfinite(settings.bin_fraction) || settings.bin_fraction < 0.0)
        throw std::invalid_argument("invalid tracker settings or sample rate");

    std::vector<Track> tracks;
    PeakAssociator association(settings);
    for (const auto& frame : frames) {
        association.push(frame, [&](std::size_t id, const TrackObservation& observation) {
            if (id > tracks.size()) {
                tracks.emplace_back();
                tracks.back().id = id;
            }
            tracks[id - 1].observations.push_back(observation);
        }, [](std::size_t) {});
    }
    const auto before = tracks.size();
    std::erase_if(tracks, [&](const Track& t) { return t.observations.size() < settings.min_observed_frames; });
    if (suppressed) *suppressed = before - tracks.size();
    for (auto& track : tracks)
        summarize(track, sample_rate_hz);
    return tracks;
}

} // namespace humtrace
