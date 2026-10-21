#include "humtrace/compare.hpp"
#include "humtrace/spectrum.hpp"
#include "humtrace/tracker.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
using humtrace::LocalPeak;
constexpr double rate = 8000.0;
constexpr double spacing = 1.0;

humtrace::PeakTracker make_tracker(std::size_t max_gap = 1, std::size_t min_obs = 3) {
    humtrace::TrackerSettings settings;
    settings.max_gap_frames = max_gap;
    settings.min_observations = min_obs;
    return humtrace::PeakTracker(settings, spacing, rate);
}

void finds_interpolated_peaks_in_synthetic_spectrum() {
    std::vector<double> samples(4096);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double t = static_cast<double>(i) / rate;
        samples[i] = 0.5 * std::sin(2 * std::numbers::pi * 120.3 * t) +
                     0.1 * std::sin(2 * std::numbers::pi * 700.0 * t);
    }
    const auto peaks = humtrace::find_local_peaks(humtrace::analyze_spectrum(samples, rate));
    require(peaks.size() == 2, "two tones give two peaks");
    require(std::abs(peaks[0].frequency_hz - 120.3) < 0.1, "strongest peak frequency interpolated");
    require(std::abs(peaks[1].frequency_hz - 700.0) < 0.1, "second peak frequency");
    require(peaks[0].level_dbfs > peaks[1].level_dbfs, "sorted strongest first");
    require(peaks[0].prominence_db > 30.0, "prominence is high");
    std::vector<double> silence(1024, 0.0);
    require(humtrace::find_local_peaks(humtrace::analyze_spectrum(silence, rate)).empty(), "silence has no peaks");
}

void tracks_steady_tone_and_drift() {
    auto tracker = make_tracker();
    for (std::uint64_t f = 0; f < 10; ++f)
        tracker.add_frame(f * 8000, {LocalPeak{100.0 + 0.1 * static_cast<double>(f), -20.0, 40.0}});
    const auto tracks = tracker.finish();
    require(tracks.size() == 1, "one track");
    require(tracks[0].observations.size() == 10 && tracks[0].missing_frames == 0, "all frames observed");
    require(std::abs(tracks[0].drift_hz_per_second - 0.1) < 1e-9, "drift 0.1 Hz/s (1 s hop)");
    require(tracks[0].end_reason == "end_of_input", "ended by input");
}

void gap_is_explicit_and_not_interpolated() {
    auto tracker = make_tracker(1, 3);
    const LocalPeak p{100.0, -20.0, 40.0};
    tracker.add_frame(0, {p});
    tracker.add_frame(1, {p});
    tracker.add_frame(2, {});
    tracker.add_frame(3, {p});
    tracker.add_frame(4, {p});
    auto tracks = tracker.finish();
    require(tracks.size() == 1 && tracks[0].observations.size() == 4, "short gap survived");
    require(tracks[0].missing_frames == 1, "gap counted, not interpolated");

    auto splitting = make_tracker(1, 2);
    splitting.add_frame(0, {p});
    splitting.add_frame(1, {p});
    splitting.add_frame(2, {});
    splitting.add_frame(3, {});
    splitting.add_frame(4, {p});
    splitting.add_frame(5, {p});
    tracks = splitting.finish();
    require(tracks.size() == 2, "long gap splits the track");
    require(tracks[0].end_reason == "gap" && tracks[0].missing_frames == 0, "trailing gap not part of track");
}

void crossing_tones_keep_identity_and_flicker_is_dropped() {
    auto tracker = make_tracker(0, 3);
    for (std::uint64_t f = 0; f < 6; ++f) {
        std::vector<LocalPeak> peaks{{100.0, -20.0, 40.0}, {300.0, -30.0, 40.0}};
        if (f % 2 == 0) peaks.push_back({500.0 + static_cast<double>(f) * 20.0, -50.0, 15.0});  // flicker
        tracker.add_frame(f, peaks);
    }
    const auto tracks = tracker.finish();
    require(tracks.size() == 2, "flicker below min_observations is dropped");
    require(tracks[0].median_frequency_hz == 100.0 && tracks[1].median_frequency_hz == 300.0, "no channel/tone swapping");

    auto first = make_tracker();
    auto second = make_tracker();
    for (std::uint64_t f = 0; f < 5; ++f) {
        first.add_frame(f, {{100.0, -20, 40}, {100.8, -21, 40}});
        second.add_frame(f, {{100.0, -20, 40}, {100.8, -21, 40}});
    }
    const auto a = first.finish();
    const auto b = second.finish();
    require(a.size() == 2 && a.size() == b.size() && a[0].median_frequency_hz == b[0].median_frequency_hz,
            "close tones resolved deterministically");
}

void compares_tracks_without_scoring_missing_as_zero() {
    humtrace::Track t1; t1.id = 1; t1.median_frequency_hz = 60.0; t1.median_level_dbfs = -30.0;
    humtrace::Track t2; t2.id = 2; t2.median_frequency_hz = 180.0; t2.median_level_dbfs = -40.0;
    humtrace::Track r1; r1.id = 7; r1.median_frequency_hz = 60.2; r1.median_level_dbfs = -33.0;
    humtrace::Track r2; r2.id = 8; r2.median_frequency_hz = 1000.0; r2.median_level_dbfs = -50.0;
    const auto result = humtrace::compare_tracks({t1, t2}, {r1, r2}, 1.0);
    require(result.matches.size() == 1, "one match");
    require(result.matches[0].track_a_id == 1 && result.matches[0].track_b_id == 7, "pairs 60 Hz tracks");
    require(std::abs(result.matches[0].delta_frequency_hz - 0.2) < 1e-9, "delta frequency");
    require(std::abs(result.matches[0].delta_level_db + 3.0) < 1e-9, "delta level");
    require(result.matches[0].delta_cents > 0.0, "cents sign");
    require(result.unmatched_a == std::vector<std::size_t>{2} && result.unmatched_b == std::vector<std::size_t>{8},
            "unmatched tracks listed");
    const auto none = humtrace::compare_tracks({t1}, {r2}, 1.0);
    require(none.matches.empty() && none.unmatched_a.size() == 1, "no match when out of tolerance");
}
} // namespace

int main() {
    finds_interpolated_peaks_in_synthetic_spectrum();
    tracks_steady_tone_and_drift();
    gap_is_explicit_and_not_interpolated();
    crossing_tones_keep_identity_and_flicker_is_dropped();
    compares_tracks_without_scoring_missing_as_zero();
    std::cout << "All tracker tests passed.\n";
}
