#include "humtrace/tracking.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace humtrace {
namespace {
double median(std::vector<double> values) {
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const auto mid = values.size() / 2;
    return (values.size() % 2 != 0) ? values[mid] : (values[mid - 1] + values[mid]) / 2.0;
}
} // namespace

MainsProximity nearest_mains(double frequency_hz, double tolerance_hz, std::size_t max_harmonic) {
    MainsProximity best;
    double best_abs = tolerance_hz;
    if (!std::isfinite(frequency_hz) || frequency_hz <= 0.0 || !(tolerance_hz >= 0.0))
        return best;
    for (const double nominal : {50.0, 60.0}) {
        const auto n = std::llround(frequency_hz / nominal);
        if (n < 1 || static_cast<std::size_t>(n) > max_harmonic)
            continue;
        const double deviation = frequency_hz - static_cast<double>(n) * nominal;
        if (std::abs(deviation) <= best_abs) {
            best_abs = std::abs(deviation);
            best = {nominal, static_cast<std::size_t>(n), deviation};
        }
    }
    return best;
}

ToneTracker::ToneTracker(const TrackerSettings& settings, double sample_rate_hz,
                         std::size_t frame_size, std::size_t hop_size)
    : settings_(settings), sample_rate_hz_(sample_rate_hz), frame_size_(frame_size),
      hop_size_(hop_size) {
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0 || frame_size == 0 || hop_size == 0)
        throw std::invalid_argument("tracker requires positive sample rate, frame and hop");
    if (!std::isfinite(settings_.min_duration_seconds) || settings_.min_duration_seconds < 0.0 ||
        settings_.min_duration_seconds > 3600.0)
        throw std::invalid_argument("tracker minimum duration must be within 0..3600 s");
    const auto duration_frames = static_cast<std::size_t>(
        std::ceil(settings_.min_duration_seconds * sample_rate_hz / static_cast<double>(hop_size)));
    settings_.min_frames = std::max<std::size_t>({1, settings_.min_frames, duration_frames});
    const double bin = sample_rate_hz / static_cast<double>(frame_size);
    tolerance_hz_ = (std::isfinite(settings.frequency_tolerance_hz) && settings.frequency_tolerance_hz > 0.0)
                        ? settings.frequency_tolerance_hz
                        : 1.5 * bin;
}

double ToneTracker::distance(const Open& track, std::size_t frame, double frequency_hz) const {
    // Nearest of: the last observation, and a linear prediction from the last two.
    // Follows steady drift without letting one jittered frame steer the track far.
    const auto& last = track.obs.back();
    double d = std::abs(frequency_hz - last.frequency_hz);
    if (track.obs.size() >= 2) {
        const auto& prev = track.obs[track.obs.size() - 2];
        const double slope = (last.frequency_hz - prev.frequency_hz) /
                             static_cast<double>(last.frame - prev.frame);
        const double predicted = last.frequency_hz + slope * static_cast<double>(frame - last.frame);
        d = std::min(d, std::abs(frequency_hz - predicted));
    }
    return d;
}

void ToneTracker::close(Open& track) {
    if (track.obs.size() < settings_.min_frames)
        return;
    if (done_.size() >= settings_.max_tracks) {
        truncated_ = true;
        return;
    }
    const auto& first = track.obs.front();
    const auto& last = track.obs.back();
    ToneTrack out;
    out.start_sample = static_cast<std::uint64_t>(first.frame) * hop_size_;
    out.end_sample = static_cast<std::uint64_t>(last.frame) * hop_size_ + frame_size_;
    out.frames_detected = track.obs.size();
    out.frames_spanned = last.frame - first.frame + 1;
    out.persistence = static_cast<double>(out.frames_detected) / static_cast<double>(out.frames_spanned);
    std::vector<double> hz, db, prom;
    double t_mean = 0.0, f_mean = 0.0;
    out.min_frequency_hz = first.frequency_hz;
    out.max_frequency_hz = first.frequency_hz;
    out.max_level_dbfs = first.level_dbfs;
    for (const auto& o : track.obs) {
        hz.push_back(o.frequency_hz);
        db.push_back(o.level_dbfs);
        prom.push_back(o.prominence_db);
        out.min_frequency_hz = std::min(out.min_frequency_hz, o.frequency_hz);
        out.max_frequency_hz = std::max(out.max_frequency_hz, o.frequency_hz);
        out.max_level_dbfs = std::max(out.max_level_dbfs, o.level_dbfs);
        t_mean += static_cast<double>(o.frame);
        f_mean += o.frequency_hz;
    }
    const auto count = static_cast<double>(track.obs.size());
    t_mean /= count;
    f_mean /= count;
    double sxy = 0.0, sxx = 0.0;
    for (const auto& o : track.obs) {
        const double dt = static_cast<double>(o.frame) - t_mean;
        sxy += dt * (o.frequency_hz - f_mean);
        sxx += dt * dt;
    }
    const double seconds_per_frame = static_cast<double>(hop_size_) / sample_rate_hz_;
    out.drift_hz_per_second = sxx > 0.0 ? (sxy / sxx) / seconds_per_frame : 0.0;
    out.median_frequency_hz = median(std::move(hz));
    out.median_level_dbfs = median(std::move(db));
    out.median_prominence_db = median(std::move(prom));
    out.mains = nearest_mains(out.median_frequency_hz, tolerance_hz_);
    done_.push_back(out);
}

void ToneTracker::push(std::size_t frame_index, const std::vector<TonalPeak>& peaks) {
    if (has_last_frame_ && frame_index <= last_frame_)
        throw std::invalid_argument("tracker frames must be pushed in increasing order");
    has_last_frame_ = true;
    last_frame_ = frame_index;

    // Expire tracks whose gap is now too long.
    for (auto it = active_.begin(); it != active_.end();) {
        if (frame_index - it->obs.back().frame > settings_.max_gap_frames + 1) {
            close(*it);
            it = active_.erase(it);
        } else {
            ++it;
        }
    }

    // Greedy nearest-frequency assignment, deterministic tie-breaking.
    struct Pair { double distance; std::size_t peak, track; };
    std::vector<Pair> pairs;
    for (std::size_t p = 0; p < peaks.size(); ++p) {
        if (!std::isfinite(peaks[p].frequency_hz))
            continue;
        for (std::size_t t = 0; t < active_.size(); ++t) {
            const double d = distance(active_[t], frame_index, peaks[p].frequency_hz);
            if (d <= tolerance_hz_)
                pairs.push_back({d, p, t});
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) {
        if (a.distance != b.distance) return a.distance < b.distance;
        if (a.peak != b.peak) return a.peak < b.peak;
        return a.track < b.track;
    });
    std::vector<bool> peak_used(peaks.size(), false), track_used(active_.size(), false);
    for (const auto& pair : pairs) {
        if (peak_used[pair.peak] || track_used[pair.track])
            continue;
        peak_used[pair.peak] = track_used[pair.track] = true;
        const auto& pk = peaks[pair.peak];
        active_[pair.track].obs.push_back({frame_index, pk.frequency_hz, pk.level_dbfs, pk.prominence_db});
    }
    for (std::size_t p = 0; p < peaks.size(); ++p) {
        if (peak_used[p] || !std::isfinite(peaks[p].frequency_hz))
            continue;
        if (active_.size() >= settings_.max_active_tracks) {
            truncated_ = true;
            continue;
        }
        const auto& pk = peaks[p];
        active_.push_back({{{frame_index, pk.frequency_hz, pk.level_dbfs, pk.prominence_db}}});
    }
}

std::vector<ToneTrack> ToneTracker::finish() {
    for (auto& track : active_)
        close(track);
    active_.clear();
    std::sort(done_.begin(), done_.end(), [](const ToneTrack& a, const ToneTrack& b) {
        return a.start_sample != b.start_sample ? a.start_sample < b.start_sample
                                                : a.median_frequency_hz < b.median_frequency_hz;
    });
    return std::move(done_);
}

} // namespace humtrace
