#include "humtrace/tracker.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace humtrace {
void summarize_track(Track& track, double sample_rate_hz, const ObservationScan& scan,
                     const ObservationOrderStatistic& order_statistic) {
    track.observed_frames = 0;
    double sum_t = 0.0, sum_f = 0.0;
    scan([&](const TrackObservation& observation) {
        if (track.observed_frames++ == 0) track.first_frame = observation.frame_index;
        track.last_frame = observation.frame_index;
        sum_t += static_cast<double>(observation.start_sample) / sample_rate_hz;
        sum_f += observation.frequency_hz;
    });
    if (track.observed_frames == 0) throw std::invalid_argument("cannot summarize an empty track");
    track.missing_frames = track.last_frame - track.first_frame + 1 - track.observed_frames;
    auto quantile = [&](unsigned quarter, bool frequency) {
        const auto span = track.observed_frames - 1;
        const auto lower = (span / 4) * quarter + (span % 4) * quarter / 4;
        const auto remainder = (span % 4) * quarter % 4;
        const auto lo = order_statistic(lower, frequency);
        if (quarter == 2 && remainder) return (lo + order_statistic(lower + 1, frequency)) / 2.0;
        return remainder ? lo + (order_statistic(lower + 1, frequency) - lo) * static_cast<double>(remainder) / 4.0 : lo;
    };
    track.median_frequency_hz = quantile(2, true);
    track.median_level_dbfs = quantile(2, false);
    track.frequency_q25_hz = quantile(1, true); track.frequency_q75_hz = quantile(3, true);
    track.level_q25_dbfs = quantile(1, false); track.level_q75_dbfs = quantile(3, false);
    const auto n = static_cast<double>(track.observed_frames);
    const double mean_t = sum_t / n, mean_f = sum_f / n;
    double covariance = 0.0, variance = 0.0, frequency_variance = 0.0;
    scan([&](const TrackObservation& observation) {
        const double dt = static_cast<double>(observation.start_sample) / sample_rate_hz - mean_t;
        const double df = observation.frequency_hz - mean_f;
        covariance += dt * df; variance += dt * dt; frequency_variance += df * df;
    });
    track.drift_hz_per_second = 0.0;
    track.drift_r_squared.reset(); track.drift_residual_rms_hz.reset();
    if (track.observed_frames < 2 || variance <= 0.0) return;
    track.drift_hz_per_second = covariance / variance;
    double residual = 0.0;
    scan([&](const TrackObservation& observation) {
        const auto dt = static_cast<double>(observation.start_sample) / sample_rate_hz - mean_t;
        const auto error = observation.frequency_hz - mean_f - track.drift_hz_per_second * dt;
        residual += error * error;
    });
    track.drift_residual_rms_hz = std::sqrt(residual / n);
    track.drift_r_squared = frequency_variance > 0.0 ? std::clamp(1.0 - residual / frequency_variance, 0.0, 1.0) : 1.0;
}

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
            ended(track.id, TrackEndReason::GapExceeded);
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
    for (const auto& track : active_) ended(track.id, TrackEndReason::EndOfInput);
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
        }, [&](std::size_t id, TrackEndReason reason) { tracks[id - 1].end_reason = reason; });
    }
    association.finish([&](std::size_t id, TrackEndReason reason) { tracks[id - 1].end_reason = reason; });
    const auto before = tracks.size();
    std::erase_if(tracks, [&](const Track& t) { return t.observations.size() < settings.min_observed_frames; });
    if (suppressed) *suppressed = before - tracks.size();
    for (auto& track : tracks) {
        std::vector<double> frequencies, levels;
        frequencies.reserve(track.observations.size()); levels.reserve(track.observations.size());
        for (const auto& observation : track.observations) {
            frequencies.push_back(observation.frequency_hz); levels.push_back(observation.level_dbfs);
        }
        std::sort(frequencies.begin(), frequencies.end()); std::sort(levels.begin(), levels.end());
        summarize_track(track, sample_rate_hz,
            [&](const ObservationVisitor& visit) { for (const auto& observation : track.observations) visit(observation); },
            [&](std::size_t rank, bool frequency) { return frequency ? frequencies.at(rank) : levels.at(rank); });
    }
    return tracks;
}

} // namespace humtrace
