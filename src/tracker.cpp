#include "humtrace/tracker.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

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

void PeakAssociator::push(const FramePeaks& frame, const Observe& observed, const End& ended, const Event& event) {
    // Bound scratch storage by valid peaks, not the caller's possibly huge list
    // of invalid measurements. No frame can finish with more active tracks than
    // valid peaks plus surviving unmatched endpoints.
    std::vector<std::size_t> valid_peaks;
    valid_peaks.reserve(std::min(frame.peaks.size(), max_active_));
    for (std::size_t p = 0; p < frame.peaks.size(); ++p) {
        const auto& peak = frame.peaks[p];
        if (!std::isfinite(peak.frequency_hz) || peak.frequency_hz <= 0.0 ||
            !std::isfinite(peak.level_dbfs)) continue;
        if (valid_peaks.size() == max_active_) throw std::runtime_error("active tracker memory limit exceeded");
        valid_peaks.push_back(p);
    }
    std::erase_if(active_, [&](const Active& track) {
        if (frame_index_ - track.last.frame_index - 1 > settings_.max_gap_frames) {
            ended(track.id, TrackEndReason::GapExceeded);
            return true;
        }
        return false;
    });

    // Union/find retains O(active + valid peaks) nodes, never the candidate
    // edges. Every in-radius edge matters: selecting the cheapest one would
    // conceal a possible split, merge, or crossing. Level is measured evidence,
    // not a reliable identity discriminator at such a boundary.
    const auto prior_count = active_.size();
    std::vector<std::size_t> roots(prior_count + valid_peaks.size());
    std::iota(roots.begin(), roots.end(), std::size_t{0});
    auto root = [&](std::size_t node) {
        while (roots[node] != node) {
            roots[node] = roots[roots[node]];
            node = roots[node];
        }
        return node;
    };
    constexpr auto candidate_limit = std::size_t{1} << 20;
    std::size_t candidate_count = 0;
    for (std::size_t p = 0; p < valid_peaks.size(); ++p) {
        const auto& peak = frame.peaks[valid_peaks[p]];
        for (std::size_t t = 0; t < prior_count; ++t) {
            const double prior = active_[t].last.frequency_hz;
            const double limit = std::max({settings_.fixed_hz_tolerance,
                settings_.relative_tolerance * prior, settings_.bin_fraction * settings_.bin_spacing_hz});
            if (std::abs(peak.frequency_hz - prior) > limit) continue;
            if (candidate_count == candidate_limit) throw std::runtime_error("tracker candidate limit exceeded");
            ++candidate_count;
            const auto left = root(t), right = root(prior_count + p);
            // The smallest track index is the component root, giving events a
            // stable order independent of the order in which edges are found.
            roots[std::max(left, right)] = std::min(left, right);
        }
    }
    struct Component { std::vector<std::size_t> tracks, peaks; };
    std::vector<Component> components(prior_count);
    for (std::size_t t = 0; t < prior_count; ++t) components[root(t)].tracks.push_back(t);
    for (std::size_t p = 0; p < valid_peaks.size(); ++p) {
        const auto component = root(prior_count + p);
        if (component < prior_count) components[component].peaks.push_back(p);
    }
    std::vector<bool> retired(prior_count, false);
    std::vector<std::size_t> continuation(valid_peaks.size(), prior_count);
    std::size_t retired_count = 0, continued_count = 0;
    for (const auto& component : components) {
        if (component.peaks.empty()) continue; // unmatched endpoints may survive a gap
        if (component.tracks.size() == 1 && component.peaks.size() == 1) {
            continuation[component.peaks[0]] = component.tracks[0];
            ++continued_count;
        } else {
            for (const auto t : component.tracks) { retired[t] = true; ++retired_count; }
        }
    }
    const auto new_count = valid_peaks.size() - continued_count;
    const auto surviving_count = prior_count - retired_count;
    if (new_count > max_active_ - surviving_count)
        throw std::runtime_error("active tracker memory limit exceeded");
    std::vector<std::size_t> ids(valid_peaks.size());
    for (std::size_t p = 0; p < valid_peaks.size(); ++p)
        ids[p] = continuation[p] < prior_count ? active_[continuation[p]].id : next_id_++;

    // Publish every event before any affected parent is summarized or child is
    // observed, so disk-backed consumers need only scalar per-track references.
    for (const auto& component : components) {
        if (component.peaks.empty() || !retired[component.tracks[0]]) continue;
        TrackAmbiguity ambiguity;
        ambiguity.id = next_ambiguity_id_++;
        ambiguity.frame_index = frame_index_;
        ambiguity.start_sample = frame.start_sample;
        ambiguity.kind = component.tracks.size() == 1 ? TrackAmbiguityKind::Split :
            component.peaks.size() == 1 ? TrackAmbiguityKind::Merge : TrackAmbiguityKind::Competing;
        if (event) {
            for (const auto t : component.tracks) ambiguity.parent_track_ids.push_back(active_[t].id);
            for (const auto p : component.peaks) ambiguity.child_track_ids.push_back(ids[p]);
            event(ambiguity);
        }
    }
    for (std::size_t t = 0; t < prior_count; ++t)
        if (retired[t]) ended(active_[t].id, TrackEndReason::Ambiguous);
    std::vector<Active> new_tracks;
    new_tracks.reserve(new_count);
    for (std::size_t p = 0; p < valid_peaks.size(); ++p) {
        const auto& peak = frame.peaks[valid_peaks[p]];
        const TrackObservation observation{frame_index_, frame.start_sample, peak.frequency_hz, peak.level_dbfs};
        if (continuation[p] < prior_count) active_[continuation[p]].last = observation;
        else new_tracks.push_back({ids[p], observation});
        observed(ids[p], observation);
    }
    std::size_t index = 0;
    std::erase_if(active_, [&](const Active&) { return retired[index++]; });
    active_.insert(active_.end(), new_tracks.begin(), new_tracks.end());
    ++frame_index_;
}

void PeakAssociator::finish(const End& ended) {
    for (const auto& track : active_) ended(track.id, TrackEndReason::EndOfInput);
    active_.clear();
}

std::vector<Track> track_peaks(const std::vector<FramePeaks>& frames, double sample_rate_hz,
                               const TrackerSettings& settings, std::size_t* suppressed,
                               std::vector<TrackAmbiguity>* ambiguities) {
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0 ||
        !std::isfinite(settings.fixed_hz_tolerance) || settings.fixed_hz_tolerance < 0.0 ||
        !std::isfinite(settings.relative_tolerance) || settings.relative_tolerance < 0.0 ||
        !std::isfinite(settings.bin_spacing_hz) || settings.bin_spacing_hz < 0.0 ||
        !std::isfinite(settings.bin_fraction) || settings.bin_fraction < 0.0)
        throw std::invalid_argument("invalid tracker settings or sample rate");

    std::vector<Track> tracks;
    if (ambiguities) ambiguities->clear();
    PeakAssociator association(settings);
    auto ensure_track = [&](std::size_t id) -> Track& {
        while (id > tracks.size()) {
            tracks.emplace_back();
            tracks.back().id = tracks.size();
        }
        return tracks[id - 1];
    };
    for (const auto& frame : frames) {
        association.push(frame, [&](std::size_t id, const TrackObservation& observation) {
            ensure_track(id).observations.push_back(observation);
        }, [&](std::size_t id, TrackEndReason reason) { tracks[id - 1].end_reason = reason; },
        [&](const TrackAmbiguity& ambiguity) {
            for (const auto id : ambiguity.parent_track_ids) ensure_track(id).end_ambiguity_id = ambiguity.id;
            for (const auto id : ambiguity.child_track_ids) ensure_track(id).start_ambiguity_id = ambiguity.id;
            if (ambiguities) ambiguities->push_back(ambiguity);
        });
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
