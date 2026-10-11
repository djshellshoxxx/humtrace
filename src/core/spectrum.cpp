#include "humtrace/spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace humtrace {
namespace {
void fft(std::vector<std::complex<double>>& values) {
    const std::size_t size = values.size();
    for (std::size_t i = 1, j = 0; i < size; ++i) {
        std::size_t bit = size >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(values[i], values[j]);
    }

    for (std::size_t length = 2; length <= size; length <<= 1) {
        const auto angle = -2.0 * std::numbers::pi / static_cast<double>(length);
        const std::complex<double> root(std::cos(angle), std::sin(angle));
        for (std::size_t start = 0; start < size; start += length) {
            std::complex<double> factor(1.0, 0.0);
            for (std::size_t offset = 0; offset < length / 2; ++offset) {
                const auto even = values[start + offset];
                const auto odd = values[start + offset + length / 2] * factor;
                values[start + offset] = even + odd;
                values[start + offset + length / 2] = even - odd;
                factor *= root;
            }
        }
    }
}

[[nodiscard]] double to_dbfs(double amplitude) {
    return amplitude <= 0.0 ? -std::numeric_limits<double>::infinity()
                            : 20.0 * std::log10(amplitude);
}

[[nodiscard]] double local_spectral_floor(const std::vector<SpectrumBin>& spectrum,
                                          std::size_t center) {
    constexpr std::size_t guard_bins = 2;
    constexpr std::size_t radius_bins = 8;
    std::vector<double> neighbors;
    const auto first = center > radius_bins ? center - radius_bins : 1;
    const auto last = std::min(spectrum.size() - 2, center + radius_bins);
    for (std::size_t index = first; index <= last; ++index) {
        const auto distance = index > center ? index - center : center - index;
        if (distance > guard_bins)
            neighbors.push_back(spectrum[index].level_dbfs);
    }
    if (neighbors.empty())
        return -std::numeric_limits<double>::infinity();
    std::sort(neighbors.begin(), neighbors.end());
    const auto middle = neighbors.size() / 2;
    if ((neighbors.size() & 1U) != 0)
        return neighbors[middle];
    const auto lower = neighbors[middle - 1];
    const auto upper = neighbors[middle];
    if (std::isinf(lower) && lower == upper)
        return lower;
    return (lower + upper) / 2.0;
}
} // namespace

std::vector<SpectrumBin> analyze_spectrum(const std::vector<double>& samples,
                                          double sample_rate_hz) {
    if (samples.size() < 4 || (samples.size() & (samples.size() - 1)) != 0)
        throw std::invalid_argument("sample count must be a power of two and at least 4");
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0)
        throw std::invalid_argument("sample rate must be finite and positive");

    const auto size = samples.size();
    std::vector<std::complex<double>> transformed(size);
    double window_sum = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        if (!std::isfinite(samples[i]))
            throw std::invalid_argument("samples must all be finite");
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(size - 1));
        window_sum += window;
        transformed[i] = {samples[i] * window, 0.0};
    }
    fft(transformed);

    std::vector<SpectrumBin> result;
    result.reserve(size / 2 + 1);
    for (std::size_t bin = 0; bin <= size / 2; ++bin) {
        double amplitude = std::abs(transformed[bin]) / window_sum;
        if (bin != 0 && bin != size / 2)
            amplitude *= 2.0;
        result.push_back({static_cast<double>(bin) * sample_rate_hz / static_cast<double>(size),
                          to_dbfs(amplitude)});
    }
    return result;
}

SpectralPeak strongest_peak(const std::vector<SpectrumBin>& spectrum) {
    if (spectrum.size() < 2)
        throw std::invalid_argument("spectrum has no non-DC bins");
    const auto peak = std::max_element(spectrum.begin() + 1, spectrum.end(),
        [](const auto& lhs, const auto& rhs) { return lhs.level_dbfs < rhs.level_dbfs; });
    if (std::isinf(peak->level_dbfs) && peak->level_dbfs < 0.0)
        return {0.0, peak->level_dbfs};
    const auto index = static_cast<std::size_t>(peak - spectrum.begin());
    if (index == 0 || index + 1 >= spectrum.size())
        return {peak->frequency_hz, peak->level_dbfs};
    const double left = spectrum[index - 1].level_dbfs;
    const double center = peak->level_dbfs;
    const double right = spectrum[index + 1].level_dbfs;
    if (!std::isfinite(left) || !std::isfinite(center) || !std::isfinite(right))
        return {peak->frequency_hz, peak->level_dbfs};
    const double curvature = left - 2.0 * center + right;
    if (curvature >= 0.0)
        return {peak->frequency_hz, peak->level_dbfs};
    const double offset = std::clamp(0.5 * (left - right) / curvature, -0.5, 0.5);
    const double interpolated_level = center - 0.25 * (left - right) * offset;
    const double bin_width = spectrum[index].frequency_hz - spectrum[index - 1].frequency_hz;
    return {peak->frequency_hz + offset * bin_width, interpolated_level};
}

HarmonicCandidate measure_harmonics(const std::vector<SpectrumBin>& spectrum,
                                    double nominal_hz, double support_threshold_dbfs,
                                    std::size_t max_harmonics,
                                    double minimum_prominence_db) {
    if (spectrum.size() < 2 || !std::isfinite(nominal_hz) || nominal_hz <= 0.0 ||
        !std::isfinite(support_threshold_dbfs) || max_harmonics == 0 ||
        !std::isfinite(minimum_prominence_db) || minimum_prominence_db < 0.0)
        throw std::invalid_argument("invalid spectrum or harmonic measurement settings");
    const double spacing = spectrum[1].frequency_hz - spectrum[0].frequency_hz;
    if (!std::isfinite(spacing) || spacing <= 0.0)
        throw std::invalid_argument("spectrum bins must have increasing frequencies");
    HarmonicCandidate candidate{nominal_hz, 0, {}};
    candidate.harmonics.reserve(std::min(max_harmonics, spectrum.size()));
    const double nyquist_hz = spectrum.back().frequency_hz;
    for (std::size_t harmonic = 1; harmonic <= max_harmonics; ++harmonic) {
        const double expected = nominal_hz * static_cast<double>(harmonic);
        if (expected > nyquist_hz)
            break;
        const auto index = static_cast<std::size_t>(std::llround(expected / spacing));
        if (index >= spectrum.size())
            break;
        const auto& bin = spectrum[index];
        const double floor = local_spectral_floor(spectrum, index);
        double prominence = bin.level_dbfs - floor;
        if (std::isinf(floor) && floor < 0.0 && std::isfinite(bin.level_dbfs))
            prominence = std::numeric_limits<double>::infinity();
        if (std::isinf(floor) && floor < 0.0 && std::isinf(bin.level_dbfs) && bin.level_dbfs < 0.0)
            prominence = 0.0;
        const bool supported = bin.level_dbfs >= support_threshold_dbfs &&
                               prominence >= minimum_prominence_db;
        candidate.supporting_harmonics += supported ? 1U : 0U;
        candidate.harmonics.push_back({harmonic, expected, bin.frequency_hz,
                                       bin.level_dbfs, floor, prominence, supported});
    }
    return candidate;
}

std::vector<ToneFrame> analyze_tone_timeline(const std::vector<double>& samples,
                                             double sample_rate_hz,
                                             std::size_t frame_size,
                                             std::size_t hop_size) {
    if (frame_size < 4 || (frame_size & (frame_size - 1)) != 0 || hop_size == 0 ||
        !std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0)
        throw std::invalid_argument("timeline requires a power-of-two frame and positive hop/rate");
    std::vector<ToneFrame> result;
    if (samples.size() < frame_size)
        return result;
    const auto frame_count = 1 + (samples.size() - frame_size) / hop_size;
    result.reserve(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const auto start = frame * hop_size;
        std::vector<double> block(samples.begin() + static_cast<std::ptrdiff_t>(start),
                                  samples.begin() + static_cast<std::ptrdiff_t>(start + frame_size));
        result.push_back({static_cast<std::uint64_t>(start),
                          strongest_peak(analyze_spectrum(block, sample_rate_hz))});
    }
    return result;
}

std::vector<InterferenceFrame> analyze_interference_timeline(
    const std::vector<double>& samples, double sample_rate_hz,
    std::size_t frame_size, std::size_t hop_size,
    double support_threshold_dbfs, std::size_t max_harmonics,
    double minimum_prominence_db) {
    if (!std::isfinite(support_threshold_dbfs) || max_harmonics == 0)
        throw std::invalid_argument("invalid harmonic support settings");
    if (!std::isfinite(minimum_prominence_db) || minimum_prominence_db < 0.0)
        throw std::invalid_argument("minimum harmonic prominence must be finite and non-negative");
    if (frame_size < 4 || (frame_size & (frame_size - 1)) != 0 || hop_size == 0 ||
        !std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0)
        throw std::invalid_argument("timeline requires a power-of-two frame and positive hop/rate");
    std::vector<InterferenceFrame> result;
    if (samples.size() < frame_size)
        return result;
    const auto frame_count = 1 + (samples.size() - frame_size) / hop_size;
    result.reserve(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const auto start = frame * hop_size;
        std::vector<double> block(samples.begin() + static_cast<std::ptrdiff_t>(start),
                                  samples.begin() + static_cast<std::ptrdiff_t>(start + frame_size));
        const auto spectrum = analyze_spectrum(block, sample_rate_hz);
        result.push_back({static_cast<std::uint64_t>(start), strongest_peak(spectrum),
            measure_harmonics(spectrum, 50.0, support_threshold_dbfs, max_harmonics, minimum_prominence_db),
            measure_harmonics(spectrum, 60.0, support_threshold_dbfs, max_harmonics, minimum_prominence_db)});
    }
    return result;
}

} // namespace humtrace
