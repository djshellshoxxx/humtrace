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
    { // A shared candidate cannot establish which old track continued.
        std::vector<TrackAmbiguity> events;
        auto tracks = track_peaks({frame(0, {{59.0, -6}, {61.0, -6}}), frame(1, {{60.0, -6}})}, 1000.0, settings, nullptr, &events);
        CHECK(tracks.size() == 3);
        CHECK(tracks[0].observed_frames == 1 && tracks[1].observed_frames == 1);
        CHECK(tracks[2].observed_frames == 1 && tracks[2].first_frame == 1);
        CHECK(tracks[0].end_reason == TrackEndReason::Ambiguous);
        CHECK(tracks[1].end_reason == TrackEndReason::Ambiguous);
        CHECK(events.size() == 1 && events[0].kind == TrackAmbiguityKind::Merge);
        CHECK(events[0].id == 1 && events[0].frame_index == 1 && events[0].start_sample == 1);
        CHECK(events[0].parent_track_ids == std::vector<std::size_t>({1, 2}));
        CHECK(events[0].child_track_ids == std::vector<std::size_t>({3}));
        CHECK(tracks[0].end_ambiguity_id == 1 && tracks[1].end_ambiguity_id == 1);
        CHECK(tracks[2].start_ambiguity_id == 1 && !tracks[2].end_ambiguity_id);
    }
    { // A split preserves both current measurements without selecting an old identity.
        const auto tracks = track_peaks({frame(0, {{60.0, -6}}),
            frame(1, {{59.5, -6}, {60.5, -30}})}, 1000.0, settings);
        CHECK(tracks.size() == 3);
        CHECK(tracks[0].observations.size() == 1);
        CHECK(tracks[1].observations[0].frequency_hz == 59.5);
        CHECK(tracks[2].observations[0].frequency_hz == 60.5);
        CHECK(tracks[2].observations[0].level_dbfs == -30.0);
    }
    { // Crossing through an unresolved peak never carries the entering identities out.
        std::vector<TrackAmbiguity> events;
        const auto tracks = track_peaks({frame(0, {{58, -6}, {62, -30}}),
            frame(1, {{59, -6}, {61, -30}}), frame(2, {{60, -6}}),
            frame(3, {{59, -30}, {61, -6}}), frame(4, {{58, -30}, {62, -6}})}, 1000.0, settings, nullptr, &events);
        CHECK(tracks.size() == 5);
        CHECK(tracks[0].first_frame == 0 && tracks[0].last_frame == 1);
        CHECK(tracks[1].first_frame == 0 && tracks[1].last_frame == 1);
        CHECK(tracks[2].first_frame == 2 && tracks[2].last_frame == 2);
        CHECK(tracks[3].first_frame == 3 && tracks[3].last_frame == 4);
        CHECK(tracks[4].first_frame == 3 && tracks[4].last_frame == 4);
        CHECK(events.size() == 2);
        CHECK(events[0].kind == TrackAmbiguityKind::Merge && events[0].frame_index == 2);
        CHECK(events[1].kind == TrackAmbiguityKind::Split && events[1].frame_index == 3);
        CHECK(events[1].parent_track_ids == std::vector<std::size_t>({3}));
        CHECK(events[1].child_track_ids == std::vector<std::size_t>({4, 5}));
        CHECK(tracks[2].start_ambiguity_id == 1 && tracks[2].end_ambiguity_id == 2);
    }
    { // Overlapping candidate radii stay ambiguous even with unequal costs/levels.
        std::vector<TrackAmbiguity> events;
        const auto tracks = track_peaks({frame(0, {{59.5, -6}, {61, -30}}),
            frame(1, {{59.75, -6}, {60.5, -30}})}, 1000.0, settings, nullptr, &events);
        CHECK(tracks.size() == 4);
        CHECK(events.size() == 1 && events[0].kind == TrackAmbiguityKind::Competing);
        CHECK(events[0].parent_track_ids == std::vector<std::size_t>({1, 2}));
        CHECK(events[0].child_track_ids == std::vector<std::size_t>({3, 4}));
    }
    { // Equal-frequency and equal-level ties never select a surviving old identity.
        std::vector<TrackAmbiguity> events;
        const auto tracks = track_peaks({frame(0, {{60, -6}, {60, -6}}),
            frame(1, {{60, -6}, {60, -6}})}, 1000.0, settings, nullptr, &events);
        CHECK(tracks.size() == 4 && events.size() == 1);
        CHECK(events[0].kind == TrackAmbiguityKind::Competing);
        CHECK(tracks[2].id == 3 && tracks[3].id == 4);
    }
    { // Retiring one component keeps unrelated continuations and gap survivors.
        std::vector<TrackAmbiguity> events;
        const auto tracks = track_peaks({frame(0, {{60, -6}, {90, -6}, {120, -6}}),
            frame(1, {{59, -6}, {0, -6}, {61, -6}, {90, -60}}),
            frame(2, {{58, -6}, {62, -6}, {90, -6}, {120, -6}})}, 1000.0, settings, nullptr, &events);
        CHECK(tracks.size() == 5 && events.size() == 1);
        CHECK(events[0].child_track_ids == std::vector<std::size_t>({4, 5}));
        CHECK(tracks[1].id == 2 && tracks[1].observed_frames == 3);
        CHECK(tracks[1].observations[1].level_dbfs == -60.0);
        CHECK(tracks[2].id == 3 && tracks[2].observed_frames == 2 && tracks[2].missing_frames == 1);
        CHECK(tracks[3].id == 4 && tracks[3].observations[1].frequency_hz == 58.0);
        CHECK(tracks[4].id == 5 && tracks[4].observations[1].frequency_hz == 62.0);
    }
    { // Expired parents do not enter a later merge; an allowed gap can.
        std::vector<TrackAmbiguity> events;
        const auto tracks = track_peaks({frame(0, {{59, -6}, {61, -6}}), frame(1, {}),
            frame(2, {{60, -6}})}, 1000.0, settings, nullptr, &events);
        CHECK(tracks.size() == 3 && events.size() == 1);
        CHECK(tracks[0].last_frame == 0 && tracks[0].missing_frames == 0);
        CHECK(events[0].frame_index == 2);
        const auto expired = track_peaks({frame(0, {{59, -6}, {61, -6}}), frame(1, {}),
            frame(2, {}), frame(3, {{60, -6}})}, 1000.0, settings, nullptr, &events);
        CHECK(expired.size() == 3 && events.empty());
        CHECK(expired[0].end_reason == TrackEndReason::GapExceeded);
        CHECK(!expired[2].start_ambiguity_id);
    }
    { // Events survive segment suppression and identify the suppressed local IDs.
        TrackerSettings strict = settings;
        strict.min_observed_frames = 2;
        std::size_t suppressed = 0;
        std::vector<TrackAmbiguity> events;
        const auto tracks = track_peaks({frame(0, {{59, -6}, {61, -6}}),
            frame(1, {{60, -6}})}, 1000.0, strict, &suppressed, &events);
        CHECK(tracks.empty() && suppressed == 3 && events.size() == 1);
        CHECK(events[0].parent_track_ids == std::vector<std::size_t>({1, 2}));
    }
    { // Streaming callbacks see lineage before any affected summary/observation.
        PeakAssociator associator(settings, 2);
        std::vector<std::size_t> callbacks;
        auto observe = [&](std::size_t id, const TrackObservation&) { callbacks.push_back(id); };
        auto end = [&](std::size_t id, TrackEndReason reason) {
            CHECK(reason == TrackEndReason::Ambiguous);
            callbacks.push_back(10 + id);
        };
        associator.push(frame(0, {{59, -6}, {61, -6}}), observe, end);
        callbacks.clear();
        associator.push(frame(1, {{60, -6}}), observe, end, [&](const TrackAmbiguity& event) {
            CHECK(event.id == 1 && event.kind == TrackAmbiguityKind::Merge);
            callbacks.push_back(100);
        });
        CHECK(callbacks == std::vector<std::size_t>({100, 11, 12, 3}));
        CHECK(associator.active_count() == 1 && associator.tracks_created() == 3);
        associator.push(frame(2, {{59, -6}, {61, -6}}), observe, end);
        CHECK(associator.active_count() == 2 && associator.tracks_created() == 5);
    }
    { // Independent components get deterministic local event IDs, not global IDs.
        std::vector<TrackAmbiguity> events;
        const std::vector<FramePeaks> input{frame(0, {{60, -6}, {90, -6}}),
            frame(1, {{90.5, -6}, {59.5, -6}, {89.5, -6}, {60.5, -6}})};
        const auto tracks = track_peaks(input, 1000.0, settings, nullptr, &events);
        CHECK(tracks.size() == 6 && events.size() == 2);
        CHECK(events[0].id == 1 && events[0].parent_track_ids == std::vector<std::size_t>({1}));
        CHECK(events[0].child_track_ids == std::vector<std::size_t>({4, 6}));
        CHECK(events[1].id == 2 && events[1].parent_track_ids == std::vector<std::size_t>({2}));
        CHECK(events[1].child_track_ids == std::vector<std::size_t>({3, 5}));
        std::vector<TrackAmbiguity> repeated;
        const auto again = track_peaks(input, 1000.0, settings, nullptr, &repeated);
        CHECK(again.size() == 6 && repeated.size() == 2 && repeated[0].id == 1);
        CHECK(repeated[0].child_track_ids == events[0].child_track_ids);
    }
    { // The active bound applies after retiring parents, and rejects excess peaks.
        PeakAssociator associator(settings, 2);
        std::size_t observations = 0;
        auto observe = [&](std::size_t, const TrackObservation&) { ++observations; };
        auto end = [](std::size_t, TrackEndReason) {};
        associator.push(frame(0, {{60, -6}}), observe, end);
        associator.push(frame(1, {{59.5, -6}, {60.5, -6}}), observe, end);
        CHECK(observations == 3 && associator.active_count() == 2);
        bool threw = false;
        try { associator.push(frame(2, {{59, -6}, {60, -6}, {61, -6}}), observe, end); }
        catch (const std::runtime_error&) { threw = true; }
        CHECK(threw && observations == 3 && associator.active_count() == 2);
    }
    { // Dense candidate graphs fail at the edge cap without retained edge storage.
        PeakAssociator associator(settings, 1025);
        const FramePeaks dense{0, std::vector<TrackPeak>(1025, {60, -6})};
        auto observe = [](std::size_t, const TrackObservation&) {};
        auto end = [](std::size_t, TrackEndReason) {};
        associator.push(dense, observe, end);
        bool threw = false;
        try { associator.push(dense, observe, end); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw && associator.active_count() == 1025 && associator.tracks_created() == 1025);
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
