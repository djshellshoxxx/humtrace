#pragma once

#include "humtrace/levels.hpp"

#include <optional>
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
    double raw_bin_frequency_hz{};
    double bin_spacing_hz{};
    // Symmetric Hann first-zero-to-first-zero main-lobe width, not estimator error.
    double hann_main_lobe_width_hz{};
    double local_floor_dbfs{};
    double prominence_db{};
    bool local_floor_available{};
    bool interpolation_applied{};
    bool boundary_peak{};
    bool competing_peak{};
    // Half-power (-3.0103 dB) width measured from neighboring spectral bins.
    std::optional<double> width_hz{};
    // Uncharacterized until a validated error model exists; never filled from bin spacing.
    std::optional<double> frequency_uncertainty_hz{};
};

struct PeakSearchSettings {
    double minimum_frequency_hz{0.0};
    // Zero selects Nyquist. Otherwise must exceed minimum_frequency_hz.
    double maximum_frequency_hz{0.0};
    double minimum_separation_hz{0.0};
    std::size_t floor_guard_bins{2};
    std::size_t floor_radius_bins{8};
};

void validate_peak_search_settings(const PeakSearchSettings& settings);

struct HarmonicMeasurement {
    std::size_t harmonic_number{};
    double expected_frequency_hz{};
    double measured_bin_hz{};
    double level_dbfs{};
    double local_floor_dbfs{};
    double prominence_db{};
    bool supports_threshold{};
    double frequency_hz{};
    double search_tolerance_hz{};
    bool local_floor_available{};
    bool boundary_peak{};
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
    std::optional<SampleLevels> levels{};
};

// Analyze a finite, power-of-two block using a Hann window and a radix-2 FFT.
// Levels are peak-amplitude dBFS; interior one-sided bins are gain-corrected. The block mean
// (DC offset) is removed before windowing by default; the input is never changed.
[[nodiscard]] std::vector<SpectrumBin> analyze_spectrum(
    const std::vector<double>& samples, double sample_rate_hz, bool remove_mean = true);

// Return the strongest non-DC bin. Throws std::invalid_argument when absent.
[[nodiscard]] SpectralPeak strongest_peak(const std::vector<SpectrumBin>& spectrum,
                                          const PeakSearchSettings& search = {});

// Local-maximum peaks (interpolated) at or above min_level_dbfs whose level exceeds the
// local spectral floor by min_prominence_db; at most max_peaks strongest, ascending frequency.
[[nodiscard]] std::vector<SpectralPeak> find_peaks(
    const std::vector<SpectrumBin>& spectrum, double min_level_dbfs, double min_prominence_db,
    std::size_t max_peaks, const PeakSearchSettings& search = {});

// Measure the nearest FFT bin at each integer multiple of nominal_hz through
// Nyquist. Threshold support is a measurement aid, not a source classification.
[[nodiscard]] HarmonicCandidate measure_harmonics(
    const std::vector<SpectrumBin>& spectrum, double nominal_hz,
    double support_threshold_dbfs, std::size_t max_harmonics,
    double minimum_prominence_db = 10.0, double harmonic_tolerance_hz = 0.0,
    const PeakSearchSettings& search = {});

// Strongest peak plus 50/60 Hz harmonic measurements for one already-computed spectrum.
[[nodiscard]] InterferenceFrame measure_frame(
    const std::vector<SpectrumBin>& spectrum, std::uint64_t start_sample,
    double support_threshold_dbfs = -60.0, std::size_t max_harmonics = 10,
    double minimum_prominence_db = 10.0, double harmonic_tolerance_hz = 0.0,
    const PeakSearchSettings& search = {});

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
