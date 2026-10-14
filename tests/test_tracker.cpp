#include "humtrace/tracker.hpp"

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
    { // invalid settings rejected
        bool threw = false;
        try { (void)track_peaks({}, 0.0, settings); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }
    std::cout << "All tracker tests passed.\n";
}
