#include "humtrace/spectrum.hpp"
#include "humtrace/tracks.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
constexpr double sample_rate = 4096.0;
constexpr std::size_t frame_size = 4096;
constexpr std::size_t hop_size = 2048;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// Analyze complete frames the same way the CLI does, without any I/O.
std::vector<humtrace::PeakFrame> peak_timeline(const std::vector<double>& samples) {
    humtrace::PeakPickingSettings picking;
    std::vector<humtrace::PeakFrame> frames;
    for (std::size_t start = 0; start + frame_size <= samples.size(); start += hop_size) {
        const std::vector<double> block(samples.begin() + static_cast<std::ptrdiff_t>(start),
                                        samples.begin() + static_cast<std::ptrdiff_t>(start + frame_size));
        const auto spectrum = humtrace::analyze_spectrum(block, sample_rate);
        frames.push_back({start, humtrace::find_spectral_peaks(spectrum, picking)});
    }
    return frames;
}

std::vector<double> tones(std::size_t count, const std::vector<std::pair<double, double>>& components) {
    std::vector<double> samples(count, 0.0);
    for (std::size_t i = 0; i < count; ++i) {
        const double time = static_cast<double>(i) / sample_rate;
        for (const auto& [frequency_hz, amplitude] : components)
            samples[i] += amplitude * std::sin(2.0 * std::numbers::pi * frequency_hz * time);
    }
    return samples;
}

void steady_tone_forms_one_track() {
    const auto frames = peak_timeline(tones(32768, {{60.0, 0.5}}));
    const auto result = humtrace::associate_peak_tracks(frames, sample_rate, {}, 1);
    require(result.size() == 1, "steady 60 Hz tone forms exactly one track");
    const auto& track = result[0];
    require(track.channel_number == 1 && track.track_number == 1, "track carries channel and number");
    require(track.observations.size() == frames.size(), "steady tone is observed in every frame");
    require(track.missing_frames == 0, "steady tone has no missing frames");
    require(std::abs(track.mean_frequency_hz - 60.0) < 0.05, "steady tone mean frequency is near 60 Hz");
    require(std::abs(track.frequency_drift_hz) < 0.05, "steady tone has negligible drift");
    require(std::abs(track.mean_level_dbfs - (-6.02)) < 0.2, "track level is the measured peak level");
    require(track.max_level_dbfs >= track.mean_level_dbfs, "max level bounds the mean level");
    require(track.min_frequency_hz <= track.mean_frequency_hz &&
            track.mean_frequency_hz <= track.max_frequency_hz, "frequency statistics are ordered");
}

void two_simultaneous_tones_form_two_tracks() {
    const auto frames = peak_timeline(tones(32768, {{60.0, 0.25}, {150.0, 0.25}}));
    const auto result = humtrace::associate_peak_tracks(frames, sample_rate, {}, 2);
    require(result.size() == 2, "two simultaneous tones form two tracks");
    // Equal-amplitude peaks may rank either way in frame 0, so match tracks by frequency.
    const auto& low = std::abs(result[0].mean_frequency_hz - 60.0) < 1.0 ? result[0] : result[1];
    const auto& high = &low == &result[0] ? result[1] : result[0];
    require(std::abs(low.mean_frequency_hz - 60.0) < 0.05, "one track follows the 60 Hz tone");
    require(std::abs(high.mean_frequency_hz - 150.0) < 0.05, "the other track follows the 150 Hz tone");
    require(result[0].track_number != result[1].track_number, "tracks have distinct numbers");
    require(low.observations.size() == frames.size() && high.observations.size() == frames.size(),
            "each tone is observed in every frame without cross-association");
}

void drifting_tone_reports_drift_rate() {
    // Linear chirp: 60 Hz at t = 0 rising at 0.125 Hz/s, so 61 Hz at 8 s.
    constexpr double rate_hz_per_s = 0.125;
    std::vector<double> samples(32768);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double t = static_cast<double>(i) / sample_rate;
        samples[i] = 0.5 * std::sin(2.0 * std::numbers::pi * (60.0 * t + 0.5 * rate_hz_per_s * t * t));
    }
    const auto result = humtrace::associate_peak_tracks(peak_timeline(samples), sample_rate, {}, 1);
    require(result.size() == 1, "slow drift stays within the frequency tolerance as one track");
    require(std::abs(result[0].drift_rate_hz_per_second - rate_hz_per_s) < 0.02,
            "drift rate estimate is within tolerance of the chirp rate");
    require(std::abs(result[0].frequency_drift_hz - rate_hz_per_s * 7.0) < 0.1,
            "first-to-last frequency change matches the chirp");
}

void noise_only_forms_no_tracks() {
    std::vector<double> samples(32768);
    std::uint32_t state = 0x2545F491U;
    for (auto& sample : samples) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        sample = (static_cast<double>(state) / 4294967295.0 - 0.5) * 0.2;
    }
    const auto result = humtrace::associate_peak_tracks(peak_timeline(samples), sample_rate, {}, 1);
    require(result.empty(), "noise-only input yields no persistent tracks");
}

// Observed frames {0-3, 5-7, 10-11}: one missing frame at 4, two missing at 8-9.
std::vector<humtrace::PeakFrame> gapped_tone_frames() {
    const std::vector<std::size_t> observed = {0, 1, 2, 3, 5, 6, 7, 10, 11};
    std::vector<humtrace::PeakFrame> frames(12);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        frames[i].start_sample = i * hop_size;
        for (const auto index : observed)
            if (index == i)
                frames[i].peaks = {{60.0, -10.0, 20.0}};
    }
    return frames;
}

void one_frame_gap_is_bridged_and_two_frame_gap_splits() {
    humtrace::TrackSettings settings;
    settings.max_gap_frames = 1;
    settings.min_observed_frames = 2;
    const auto result = humtrace::associate_peak_tracks(gapped_tone_frames(), sample_rate, settings, 1);
    require(result.size() == 2, "a one-frame gap is bridged and a two-frame gap starts a new track");
    require(result[0].observations.size() == 7, "bridged track keeps only observed frames");
    require(result[0].missing_frames == 1, "bridged gap is counted as missing, not interpolated");
    require(result[0].start_sample == 0 && result[0].end_sample == 7 * hop_size,
            "first track spans its first and last observed frames");
    require(result[1].observations.size() == 2 && result[1].missing_frames == 0 &&
            result[1].start_sample == 10 * hop_size, "second track starts at its first observation");
}

void zero_gap_splits_every_gap() {
    humtrace::TrackSettings settings;
    settings.max_gap_frames = 0;
    settings.min_observed_frames = 2;
    const auto result = humtrace::associate_peak_tracks(gapped_tone_frames(), sample_rate, settings, 1);
    require(result.size() == 3, "with no allowed gap each interruption starts a track");
    require(result[0].observations.size() == 4 && result[1].observations.size() == 3 &&
            result[2].observations.size() == 2, "split tracks keep their observed frames");
}

void minimum_persistence_filters_short_tracks() {
    humtrace::TrackSettings settings;
    settings.max_gap_frames = 1;
    settings.min_observed_frames = 3;
    const auto result = humtrace::associate_peak_tracks(gapped_tone_frames(), sample_rate, settings, 1);
    require(result.size() == 1, "tracks below the minimum persistence are not reported");
    require(result[0].observations.size() == 7, "the persistent track is retained");
}

void frequency_jump_splits_track() {
    std::vector<humtrace::PeakFrame> frames(6);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        frames[i].start_sample = i * hop_size;
        frames[i].peaks = {{i < 3 ? 60.0 : 65.0, -10.0, 20.0}};
    }
    humtrace::TrackSettings settings;
    settings.frequency_tolerance_hz = 1.0;
    settings.min_observed_frames = 3;
    const auto result = humtrace::associate_peak_tracks(frames, sample_rate, settings, 1);
    require(result.size() == 2, "a sustained 5 Hz jump exceeds tolerance and starts a new track");
    settings.frequency_tolerance_hz = 6.0;
    require(humtrace::associate_peak_tracks(frames, sample_rate, settings, 1).size() == 1,
            "the same jump is one track when tolerance covers it");
}

void association_is_deterministic_and_validated() {
    const auto frames = peak_timeline(tones(32768, {{60.0, 0.25}, {150.0, 0.25}}));
    const auto first = humtrace::associate_peak_tracks(frames, sample_rate, {}, 1);
    const auto second = humtrace::associate_peak_tracks(frames, sample_rate, {}, 1);
    require(first.size() == second.size(), "repeated association gives the same track count");
    for (std::size_t i = 0; i < first.size(); ++i)
        require(first[i].mean_frequency_hz == second[i].mean_frequency_hz &&
                first[i].observations.size() == second[i].observations.size(),
                "repeated association gives identical tracks");

    bool threw = false;
    auto unordered = gapped_tone_frames();
    std::swap(unordered[0], unordered[1]);
    try {
        (void)humtrace::associate_peak_tracks(unordered, sample_rate, {}, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "frames out of order are rejected");

    threw = false;
    humtrace::TrackSettings invalid;
    invalid.frequency_tolerance_hz = std::numeric_limits<double>::quiet_NaN();
    try {
        (void)humtrace::associate_peak_tracks(frames, sample_rate, invalid, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "non-finite tolerance is rejected");
}
} // namespace

int main() {
    steady_tone_forms_one_track();
    two_simultaneous_tones_form_two_tracks();
    drifting_tone_reports_drift_rate();
    noise_only_forms_no_tracks();
    one_frame_gap_is_bridged_and_two_frame_gap_splits();
    zero_gap_splits_every_gap();
    minimum_persistence_filters_short_tracks();
    frequency_jump_splits_track();
    association_is_deterministic_and_validated();
    std::cout << "All track tests passed.\n";
}
