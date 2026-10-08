#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace humtrace {

struct SpectrumBin {
    double frequency_hz{};
    double level_dbfs{};
};

struct SpectralPeak {
    double frequency_hz{};
    double level_dbfs{};
};

struct HarmonicMeasurement {
    std::size_t harmonic_number{};
    double expected_frequency_hz{};
    double measured_bin_hz{};
    double level_dbfs{};
    double local_floor_dbfs{};
    double prominence_db{};
    bool supports_threshold{};
};

struct HarmonicCandidate {
    double nominal_frequency_hz{};
    std::size_t supporting_harmonics{};
    std::vector<HarmonicMeasurement> harmonics;
};

struct ToneFrame {
    std::uint64_t start_sample{};
    SpectralPeak strongest_peak;
};

struct InterferenceFrame {
    std::uint64_t start_sample{};
    SpectralPeak strongest_peak;
    HarmonicCandidate mains_50;
    HarmonicCandidate mains_60;
};

// Analyze a finite, power-of-two block using a Hann window and a radix-2 FFT.
// Levels are peak-amplitude dBFS; interior one-sided bins are gain-corrected.
[[nodiscard]] std::vector<SpectrumBin> analyze_spectrum(
    const std::vector<double>& samples, double sample_rate_hz);

// Return the strongest non-DC bin. Throws std::invalid_argument when absent.
[[nodiscard]] SpectralPeak strongest_peak(const std::vector<SpectrumBin>& spectrum);

// Measure the nearest FFT bin at each integer multiple of nominal_hz through
// Nyquist. Threshold support is a measurement aid, not a source classification.
[[nodiscard]] HarmonicCandidate measure_harmonics(
    const std::vector<SpectrumBin>& spectrum, double nominal_hz,
    double support_threshold_dbfs, std::size_t max_harmonics,
    double minimum_prominence_db = 10.0);

// Measure the strongest non-DC component in complete, fixed-size frames.
[[nodiscard]] std::vector<ToneFrame> analyze_tone_timeline(
    const std::vector<double>& samples, double sample_rate_hz,
    std::size_t frame_size, std::size_t hop_size);

// Measure per-frame dominant bins and nominal 50/60 Hz harmonic bins.
[[nodiscard]] std::vector<InterferenceFrame> analyze_interference_timeline(
    const std::vector<double>& samples, double sample_rate_hz,
    std::size_t frame_size, std::size_t hop_size,
    double support_threshold_dbfs = -60.0, std::size_t max_harmonics = 10,
    double minimum_prominence_db = 10.0);

} // namespace humtrace
