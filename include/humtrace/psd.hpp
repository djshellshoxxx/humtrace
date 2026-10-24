#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace humtrace {

// Welch power spectral density (PSD) estimator with a Hann window.
//
// Units: density is reported in dBFS^2/Hz, defined as 10*log10(P / (1 FS^2 per Hz)),
// where P is the one-sided power spectral density of samples normalized to full scale 1.0.
// This is a power-per-hertz quantity and is NOT the peak-amplitude dBFS used by
// spectrum.hpp (20*log10 of amplitude). The two scales differ in both the power/amplitude
// convention and the bin width: a bin-centred sine of amplitude A has a PSD peak of
// A^2*C^2 / (2*fs*S2) (C = sum of window, S2 = sum of squared window), so PSD values
// must not be compared with peak dBFS values or with each other across different
// segment sizes without this conversion. For white noise of variance s^2 the one-sided
// density is 2*s^2/fs, independent of segment size.
//
// Memory is bounded by one segment (samples, FFT scratch and an accumulator of
// segment_size/2 + 1 values), independent of the stream length.

inline constexpr std::size_t max_welch_segment_size = std::size_t{1} << 20;

struct WelchSettings {
    // Power of two from 4 to max_welch_segment_size.
    std::size_t segment_size{4096};
    // From 1 to segment_size. Overlap is segment_size - hop_size.
    std::size_t hop_size{2048};
    // Subtract each segment's mean before windowing. False preserves the original PSD convention.
    bool remove_mean{false};
};

struct PsdBin {
    double frequency_hz{};
    // -infinity for exactly zero power.
    double density_dbfs2_per_hz{};
};

struct WelchResult {
    std::size_t segment_size{};
    std::size_t hop_size{};
    double bin_width_hz{};
    // Complete segments only. A trailing partial segment is never included.
    std::size_t segments_averaged{};
    std::uint64_t samples_processed{};
    // DC through Nyquist, ascending. Empty when no complete segment was processed.
    std::vector<PsdBin> bins;
};

// Streaming, single-channel estimator. Use one instance per channel. Feeding the same
// samples in any chunking yields bit-identical results to a one-shot estimate.
class WelchPsd {
public:
    // Throws std::invalid_argument for a non-finite or non-positive sample rate or
    // invalid settings.
    WelchPsd(double sample_rate_hz, WelchSettings settings);

    // Throws std::invalid_argument if any sample is not finite; in that case no
    // sample is consumed and the estimator state is unchanged.
    void push(std::span<const double> samples);

    // Discards all accumulated segments and samples, keeping the settings.
    void reset() noexcept;

    [[nodiscard]] std::size_t segments_averaged() const noexcept { return segments_; }
    [[nodiscard]] std::uint64_t samples_processed() const noexcept { return samples_processed_; }
    [[nodiscard]] double sample_rate_hz() const noexcept { return sample_rate_hz_; }
    [[nodiscard]] const WelchSettings& settings() const noexcept { return settings_; }

    // Returns the average of the complete segments processed so far. Does not change
    // the estimator, so more samples may be pushed afterwards.
    [[nodiscard]] WelchResult finish() const;

private:
    void process_segment();

    double sample_rate_hz_{};
    WelchSettings settings_{};
    std::vector<double> window_;
    double density_scale_{};
    std::vector<double> ring_;
    std::vector<double> accumulator_;
    std::vector<std::complex<double>> work_;
    std::size_t write_index_{};
    std::uint64_t samples_processed_{};
    std::uint64_t next_segment_start_{};
    std::size_t segments_{};
};

// One-shot convenience wrapper around WelchPsd.
[[nodiscard]] WelchResult estimate_welch_psd(std::span<const double> samples,
                                             double sample_rate_hz, WelchSettings settings);

// Settings for the local median noise floor and the nominal 50/60 Hz PSD measurements.
struct PsdNoiseFloorSettings {
    // Bins within this distance of a harmonic bin are excluded from its floor.
    std::size_t guard_bins{2};
    // The floor is the median over bins within this distance (guard excluded). Must exceed guard_bins.
    std::size_t radius_bins{8};
    std::size_t max_harmonics{10};
    // Prominence is the strongest of the two bins bracketing the nominal harmonic, minus the
    // local floor. Leakage from a stronger component in an adjacent bin can raise it.
    double minimum_prominence_db{10.0};
    // Zero preserves nominal bracketing. Positive values search local maxima within
    // max(tolerance, bin_width), capped below half the fundamental spacing.
    double harmonic_tolerance_hz{0.0};
};

struct PsdHarmonicMeasurement {
    std::size_t harmonic_number{};
    double expected_frequency_hz{};
    // Centre frequency of the strongest of the two bins bracketing the nominal harmonic.
    double measured_bin_hz{};
    double level_dbfs2_per_hz{};
    // Median density over the floor neighbourhood, in dBFS^2/Hz.
    double local_floor_dbfs2_per_hz{};
    double prominence_db{};
    // Prominence is at least minimum_prominence_db. This is a measurement flag, not attribution.
    bool prominent{};
    double frequency_hz{};
    double search_tolerance_hz{};
    bool local_floor_available{};
};

struct PsdHarmonicCandidate {
    double nominal_frequency_hz{};
    std::size_t prominent_harmonics{};
    std::vector<PsdHarmonicMeasurement> harmonics;
};

struct PsdSummary {
    std::size_t segments_averaged{};
    std::uint64_t samples_processed{};
    double bin_width_hz{};
    // Median density over all non-DC, non-Nyquist bins.
    double broadband_median_dbfs2_per_hz{};
    PsdHarmonicCandidate mains_50;
    PsdHarmonicCandidate mains_60;
};

// Measure the nominal harmonics of a PSD. Throws std::invalid_argument for invalid
// settings or a PSD without interior bins.
[[nodiscard]] PsdHarmonicCandidate measure_psd_harmonics(const WelchResult& psd,
                                                         double nominal_hz,
                                                         const PsdNoiseFloorSettings& settings);

// Returns std::nullopt when the PSD has no complete segment.
[[nodiscard]] std::optional<PsdSummary> summarize_welch_psd(const WelchResult& psd,
                                                            const PsdNoiseFloorSettings& settings);

} // namespace humtrace
