#include "humtrace/tracker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace humtrace {
namespace {
[[nodiscard]] double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return (values.size() & 1U) != 0 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}
} // namespace

std::vector<LocalPeak> find_local_peaks(const std::vector<SpectrumBin>& spectrum,
                                        const PeakSettings& settings) {
    if (!std::isfinite(settings.minimum_level_dbfs) || !std::isfinite(settings.minimum_prominence_db) ||
        settings.minimum_prominence_db < 0.0)
        throw std::invalid_argument("invalid peak settings");
    std::vector<LocalPeak> peaks;
    if (spectrum.size() < 4 || settings.max_peaks == 0)
        return peaks;
    constexpr std::size_t guard_bins = 2;
    constexpr std::size_t radius_bins = 8;
    for (std::size_t i = 2; i + 1 < spectrum.size(); ++i) {
        const double left = spectrum[i - 1].level_dbfs;
        const double center = spectrum[i].level_dbfs;
        const double right = spectrum[i + 1].level_dbfs;
        if (!std::isfinite(center) || center < settings.minimum_level_dbfs ||
            !(center > left) || !(center >= right))
            continue;
        std::vector<double> neighbors;
        const auto first = i > radius_bins ? i - radius_bins : 1;
        const auto last = std::min(spectrum.size() - 2, i + radius_bins);
        for (std::size_t j = first; j <= last; ++j) {
            const auto distance = j > i ? j - i : i - j;
            if (distance > guard_bins)
                neighbors.push_back(spectrum[j].level_dbfs);
        }
        if (neighbors.empty())
            continue;
        std::sort(neighbors.begin(), neighbors.end());
        const auto mid = neighbors.size() / 2;
        const double floor = (neighbors.size() & 1U) != 0
                                 ? neighbors[mid]
                                 : (std::isinf(neighbors[mid - 1]) ? neighbors[mid - 1]
                                                                   : (neighbors[mid - 1] + neighbors[mid]) / 2.0);
        const double prominence = std::isinf(floor) ? std::numeric_limits<double>::infinity() : center - floor;
        if (prominence < settings.minimum_prominence_db)
            continue;
        double frequency = spectrum[i].frequency_hz;
        double level = center;
        if (std::isfinite(left) && std::isfinite(right)) {
            const double curvature = left - 2.0 * center + right;
            if (curvature < 0.0) {
                const double offset = std::clamp(0.5 * (left - right) / curvature, -0.5, 0.5);
                level = center - 0.25 * (left - right) * offset;
                frequency += offset * (spectrum[i].frequency_hz - spectrum[i - 1].frequency_hz);
            }
        }
        peaks.push_back({frequency, level, prominence});
    }
    std::stable_sort(peaks.begin(), peaks.end(), [](const LocalPeak& a, const LocalPeak& b) {
        return a.level_dbfs > b.level_dbfs;
    });
    if (peaks.size() > settings.max_peaks)
        peaks.resize(settings.max_peaks);
    return peaks;
}

PeakTracker::PeakTracker(const TrackerSettings& settings, double bin_spacing_hz, double sample_rate_hz)
    : settings_(settings), bin_spacing_hz_(bin_spacing_hz), sample_rate_hz_(sample_rate_hz) {
    if (!std::isfinite(bin_spacing_hz) || bin_spacing_hz <= 0.0 || !std::isfinite(sample_rate_hz) ||
        sample_rate_hz <= 0.0 || settings.min_observations == 0 || settings.fixed_hz_tolerance < 0.0 ||
        settings.relative_tolerance < 0.0 || settings.bin_fraction_tolerance < 0.0)
        throw std::invalid_argument("invalid tracker settings");
}

void PeakTracker::close(Active& active, const char* reason) {
    auto& track = active.track;
    // Trailing missing frames are the gap that ended the track, not part of it.
    track.missing_frames -= std::min(track.missing_frames, active.trailing_missing);
    track.end_reason = reason;
    if (track.observations.size() < settings_.min_observations)
        return;
    std::vector<double> frequencies, levels;
    for (const auto& o : track.observations) {
        frequencies.push_back(o.frequency_hz);
        levels.push_back(o.level_dbfs);
    }
    track.first_sample = track.observations.front().start_sample;
    track.last_sample = track.observations.back().start_sample;
    track.median_frequency_hz = median(frequencies);
    track.median_level_dbfs = median(levels);
    if (track.observations.size() >= 2) {
        const double n = static_cast<double>(track.observations.size());
        double sum_t = 0, sum_f = 0, sum_tt = 0, sum_tf = 0;
        for (const auto& o : track.observations) {
            const double t = static_cast<double>(o.start_sample) / sample_rate_hz_;
            sum_t += t; sum_f += o.frequency_hz; sum_tt += t * t; sum_tf += t * o.frequency_hz;
        }
        const double denominator = n * sum_tt - sum_t * sum_t;
        track.drift_hz_per_second = denominator > 0.0 ? (n * sum_tf - sum_t * sum_f) / denominator : 0.0;
    }
    finished_.push_back(std::move(track));
}

void PeakTracker::add_frame(std::uint64_t start_sample, const std::vector<LocalPeak>& peaks) {
    struct Candidate { double distance; std::size_t track_id; std::size_t peak_index; std::size_t active_index; };
    std::vector<Candidate> candidates;
    for (std::size_t a = 0; a < active_.size(); ++a) {
        const auto& last = active_[a].track.observations.back();
        const double tolerance = std::max({settings_.fixed_hz_tolerance,
                                           settings_.relative_tolerance * last.frequency_hz,
                                           settings_.bin_fraction_tolerance * bin_spacing_hz_});
        for (std::size_t p = 0; p < peaks.size(); ++p) {
            const double distance = std::abs(peaks[p].frequency_hz - last.frequency_hz);
            if (distance <= tolerance)
                candidates.push_back({distance, active_[a].track.id, p, a});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& x, const Candidate& y) {
        return std::tie(x.distance, x.track_id, x.peak_index) < std::tie(y.distance, y.track_id, y.peak_index);
    });
    std::vector<bool> track_used(active_.size(), false), peak_used(peaks.size(), false);
    for (const auto& c : candidates) {
        if (track_used[c.active_index] || peak_used[c.peak_index])
            continue;
        track_used[c.active_index] = peak_used[c.peak_index] = true;
        auto& active = active_[c.active_index];
        const auto& peak = peaks[c.peak_index];
        active.track.observations.push_back({start_sample, peak.frequency_hz, peak.level_dbfs, peak.prominence_db});
        active.trailing_missing = 0;
    }
    std::vector<Active> still_active;
    for (std::size_t a = 0; a < active_.size(); ++a) {
        auto& active = active_[a];
        if (!track_used[a]) {
            ++active.trailing_missing;
            ++active.track.missing_frames;
            if (active.trailing_missing > settings_.max_gap_frames) {
                close(active, "gap");
                continue;
            }
        }
        still_active.push_back(std::move(active));
    }
    active_ = std::move(still_active);
    for (std::size_t p = 0; p < peaks.size(); ++p) {
        if (peak_used[p])
            continue;
        Active fresh;
        fresh.track.id = next_id_++;
        fresh.track.observations.push_back({start_sample, peaks[p].frequency_hz, peaks[p].level_dbfs,
                                            peaks[p].prominence_db});
        active_.push_back(std::move(fresh));
    }
}

std::vector<Track> PeakTracker::finish() {
    for (auto& active : active_)
        close(active, "end_of_input");
    active_.clear();
    std::sort(finished_.begin(), finished_.end(), [](const Track& a, const Track& b) { return a.id < b.id; });
    return std::move(finished_);
}

} // namespace humtrace
