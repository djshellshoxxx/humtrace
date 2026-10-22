#include "humtrace/tracker.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace humtrace;

#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAILED line " << __LINE__ << ": " #cond "\n"; std::exit(1); } } while (0)

static FramePeaks frame(std::uint64_t start, std::initializer_list<TrackPeak> peaks) {
    return {start, std::vector<TrackPeak>(peaks)};
}

int main() {
    TrackerSettings settings;
    settings.fixed_hz_tolerance = 1.0;
    settings.min_observed_frames = 1;

    { // steady tone with one dropout: gap explicit, not interpolated
        auto tracks = track_peaks({frame(0, {{60.0, -6}}), frame(100, {}), frame(200, {{60.2, -6}}),
                                   frame(300, {{60.3, -6}})}, 1000.0, settings);
        CHECK(tracks.size() == 1);
        CHECK(tracks[0].observed_frames == 3 && tracks[0].missing_frames == 1);
        CHECK(tracks[0].observations.size() == 3);
        CHECK(tracks[0].drift_hz_per_second > 0.99 && tracks[0].drift_hz_per_second < 1.01);
    }
    { // gap longer than limit splits the track
        auto tracks = track_peaks({frame(0, {{60.0, -6}}), frame(1, {}), frame(2, {}),
                                   frame(3, {{60.0, -6}})}, 1000.0, settings);
        CHECK(tracks.size() == 2);
        CHECK(tracks[0].end_reason == TrackEndReason::GapExceeded);
        CHECK(tracks[1].end_reason == TrackEndReason::EndOfInput);
    }
    { // two tones stay separate; one-to-one assignment
        auto tracks = track_peaks({frame(0, {{50.0, -6}, {60.0, -9}}),
                                   frame(1, {{50.1, -6}, {60.1, -9}})}, 1000.0, settings);
        CHECK(tracks.size() == 2);
        CHECK(tracks[0].median_frequency_hz < 51.0 && tracks[1].median_frequency_hz > 59.0);
    }
    { // abrupt jump beyond tolerance starts a new track
        auto tracks = track_peaks({frame(0, {{60.0, -6}}), frame(1, {{80.0, -6}})}, 1000.0, settings);
        CHECK(tracks.size() == 2);
    }
    { // invalid peaks ignored; deterministic repeat
        const std::vector<FramePeaks> input{frame(0, {{0.0, -6}, {60.0, -6}})};
        CHECK(track_peaks(input, 1000.0, settings).size() == 1);
        CHECK(track_peaks(input, 1000.0, settings).size() == track_peaks(input, 1000.0, settings).size());
    }
    { // minimum observed frames suppresses flicker but keeps persistent tracks
        TrackerSettings strict = settings;
        strict.min_observed_frames = 3;
        auto tracks = track_peaks({frame(0, {{60.0, -6}, {90.0, -6}}), frame(1, {{60.0, -6}}), frame(2, {{60.1, -6}, {200.0, -6}})}, 1000.0, strict);
        CHECK(tracks.size() == 1 && tracks[0].observed_frames == 3);
    }
    { // equal-distance candidates resolve by track id (older track wins), deterministically
        auto tracks = track_peaks({frame(0, {{59.0, -6}, {61.0, -6}}), frame(1, {{60.0, -6}})}, 1000.0, settings);
        CHECK(tracks.size() == 2);
        CHECK(tracks[0].observed_frames == 2 && tracks[1].observed_frames == 1);
    }
    { // even-count medians average the two middle values
        auto tracks = track_peaks({frame(0, {{60.0, -10}}), frame(1, {{60.2, -20}}), frame(2, {{60.4, -30}}), frame(3, {{60.6, -40}})}, 1000.0, settings);
        CHECK(tracks.size() == 1);
        CHECK(tracks[0].median_level_dbfs == -25.0);
        CHECK(tracks[0].level_q25_dbfs == -32.5);
        CHECK(tracks[0].level_q75_dbfs == -17.5);
        CHECK(std::abs(tracks[0].frequency_q25_hz - 60.15) < 1e-9);
        CHECK(std::abs(tracks[0].frequency_q75_hz - 60.45) < 1e-9);
        CHECK(tracks[0].drift_r_squared && *tracks[0].drift_r_squared > 0.999999);
        CHECK(tracks[0].drift_residual_rms_hz && *tracks[0].drift_residual_rms_hz < 1e-9);
        CHECK(std::abs(tracks[0].median_frequency_hz - 60.3) < 1e-9);
    }
    { // suppressed count
        TrackerSettings strict = settings;
        strict.min_observed_frames = 2;
        std::size_t suppressed = 99;
        auto tracks = track_peaks({frame(0, {{60.0, -6}, {90.0, -6}}), frame(1, {{60.0, -6}})}, 1000.0, strict, &suppressed);
        CHECK(tracks.size() == 1 && suppressed == 1);
    }
    { // an enormous gap allowance must not overflow
        TrackerSettings huge = settings;
        huge.max_gap_frames = SIZE_MAX;
        CHECK(track_peaks({frame(0, {{60.0, -6}}), frame(5, {{60.0, -6}})}, 1000.0, huge).size() == 1);
    }
    { // Constant frequency has perfect zero-residual fit; one observation has no fit.
        const auto constant = track_peaks({frame(0, {{60, -6}}), frame(100, {{60, -6}})}, 1000, settings);
        CHECK(constant[0].drift_r_squared == 1.0);
        CHECK(constant[0].drift_residual_rms_hz == 0.0);
        const auto single = track_peaks({frame(0, {{60, -6}})}, 1000, settings);
        CHECK(!single[0].drift_r_squared && !single[0].drift_residual_rms_hz);
    }
    { // invalid settings rejected
        bool threw = false;
        try { (void)track_peaks({}, 0.0, settings); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }
    std::cout << "All tracker tests passed.\n";
}
