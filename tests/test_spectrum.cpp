#include "humtrace/spectrum.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void finds_peak_frequency_and_level_for_a_sine() {
    constexpr double sample_rate = 4096.0;
    constexpr std::size_t count = 4096;
    constexpr double frequency = 60.0;
    constexpr double amplitude = 0.5;
    std::vector<double> samples(count);
    for (std::size_t i = 0; i < count; ++i)
        samples[i] = amplitude * std::sin(2.0 * 3.14159265358979323846 * frequency *
                                           static_cast<double>(i) / sample_rate);

    const auto result = humtrace::analyze_spectrum(samples, sample_rate);
    require(result.size() == count / 2 + 1, "spectrum includes DC through Nyquist");
    const auto peak = humtrace::strongest_peak(result);
    require(std::abs(peak.frequency_hz - frequency) < 0.01, "exact-bin sine frequency is measured correctly");
    require(std::abs(peak.level_dbfs - (-6.0206)) < 0.1, "sine level is normalized to dBFS");
}

void rejects_non_power_of_two_input() {
    bool threw = false;
    try {
        (void)humtrace::analyze_spectrum(std::vector<double>(1000, 0.0), 48000.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "non-power-of-two input is rejected explicitly");
}

void measures_harmonic_support_without_source_attribution() {
    constexpr std::size_t count = 4096;
    constexpr double sample_rate = 4096.0;
    std::vector<double> samples(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double time = static_cast<double>(i) / sample_rate;
        samples[i] = 0.25 * std::sin(2.0 * std::numbers::pi * 60.0 * time) +
                     0.125 * std::sin(2.0 * std::numbers::pi * 120.0 * time);
    }
    const auto spectrum = humtrace::analyze_spectrum(samples, sample_rate);
    const auto candidate = humtrace::measure_harmonics(spectrum, 60.0, -40.0, 5);
    require(candidate.harmonics.size() == 5, "all requested in-band harmonics are reported");
    require(candidate.supporting_harmonics == 2, "only components above threshold count as support");
    require(candidate.harmonics[0].measured_bin_hz == 60.0, "fundamental bin is measured");
    require(candidate.harmonics[1].measured_bin_hz == 120.0, "second harmonic bin is measured");
    require(candidate.harmonics[2].level_dbfs < -40.0, "absent harmonic remains visible as below threshold");
}

void tracks_the_strongest_component_in_non_overlapping_frames() {
    constexpr double sample_rate = 4096.0;
    constexpr std::size_t frame_size = 4096;
    std::vector<double> samples(frame_size * 2);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double time = static_cast<double>(i) / sample_rate;
        const double frequency = i < frame_size ? 60.0 : 120.0;
        samples[i] = 0.5 * std::sin(2.0 * std::numbers::pi * frequency * time);
    }
    const auto frames = humtrace::analyze_tone_timeline(samples, sample_rate, frame_size, frame_size);
    require(frames.size() == 2, "timeline reports complete analysis frames");
    require(frames[0].start_sample == 0, "first frame retains its sample position");
    require(frames[1].start_sample == frame_size, "second frame retains its sample position");
    require(std::abs(frames[0].strongest_peak.frequency_hz - 60.0) < 0.01,
            "first frame tone is tracked");
    require(std::abs(frames[1].strongest_peak.frequency_hz - 120.0) < 0.01,
            "second frame tone change is tracked");
}

void reports_50_and_60_hz_harmonic_measurements_per_frame() {
    constexpr double sample_rate = 4096.0;
    constexpr std::size_t count = 4096;
    std::vector<double> samples(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double time = static_cast<double>(i) / sample_rate;
        samples[i] = 0.25 * std::sin(2.0 * std::numbers::pi * 60.0 * time) +
                     0.125 * std::sin(2.0 * std::numbers::pi * 120.0 * time);
    }
    const auto frames = humtrace::analyze_interference_timeline(samples, sample_rate, count, count);
    require(frames.size() == 1, "one complete analysis frame is returned");
    require(frames[0].mains_60.supporting_harmonics == 2,
            "60 Hz candidate reports the observed fundamental and second harmonic");
    require(frames[0].mains_50.supporting_harmonics == 0,
            "50 Hz candidate does not count absent harmonic bins");
}

void represents_silence_without_inventing_a_peak_frequency() {
    const auto spectrum = humtrace::analyze_spectrum(std::vector<double>(4096, 0.0), 48000.0);
    const auto peak = humtrace::strongest_peak(spectrum);
    require(peak.frequency_hz == 0.0, "silence has no reported peak frequency");
    require(std::isinf(peak.level_dbfs) && peak.level_dbfs < 0.0,
            "silence is represented at negative infinity dBFS");
}
} // namespace

int main() {
    finds_peak_frequency_and_level_for_a_sine();
    rejects_non_power_of_two_input();
    measures_harmonic_support_without_source_attribution();
    tracks_the_strongest_component_in_non_overlapping_frames();
    reports_50_and_60_hz_harmonic_measurements_per_frame();
    represents_silence_without_inventing_a_peak_frequency();
    std::cout << "All spectrum tests passed.\n";
}
