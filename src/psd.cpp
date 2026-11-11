#include "humtrace/psd.hpp"
#include "fft.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace humtrace {
namespace {
constexpr double negative_infinity = -std::numeric_limits<double>::infinity();

[[nodiscard]] double to_db(double power) {
    return power <= 0.0 ? negative_infinity : 10.0 * std::log10(power);
}

void validate_estimator(double sample_rate_hz, const WelchSettings& settings) {
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0)
        throw std::invalid_argument("PSD sample rate must be finite and positive");
    const auto size = settings.segment_size;
    if (size < 4 || size > max_welch_segment_size || (size & (size - 1)) != 0)
        throw std::invalid_argument("PSD segment size must be a power of two from 4 to 1048576");
    if (settings.hop_size == 0 || settings.hop_size > size)
        throw std::invalid_argument("PSD hop size must be from 1 to the segment size");
}

void validate_floor_settings(const PsdNoiseFloorSettings& settings) {
    if (settings.max_harmonics == 0 || settings.radius_bins <= settings.guard_bins ||
        !std::isfinite(settings.minimum_prominence_db) || settings.minimum_prominence_db < 0.0)
        throw std::invalid_argument("invalid PSD harmonic settings");
}

// Symmetric Hann window, matching analyze_spectrum in spectrum.cpp.
[[nodiscard]] std::vector<double> hann_window(std::size_t size) {
    std::vector<double> window(size);
    for (std::size_t i = 0; i < size; ++i)
        window[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                         static_cast<double>(size - 1));
    return window;
}

// Median with the same -infinity handling as the spectral floor in spectrum.cpp.
[[nodiscard]] double median_of(std::vector<double>& values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if ((values.size() & 1U) != 0)
        return values[middle];
    const auto lower = values[middle - 1];
    const auto upper = values[middle];
    if (std::isinf(lower) && lower == upper)
        return lower;
    return (lower + upper) / 2.0;
}

// Difference of two dB values that stays defined when either side is -infinity.
[[nodiscard]] double prominence_of(double level_db, double floor_db) {
    if (level_db == negative_infinity && floor_db == negative_infinity)
        return 0.0;
    return level_db - floor_db;
}
} // namespace

WelchPsd::WelchPsd(double sample_rate_hz, WelchSettings settings)
    : sample_rate_hz_(sample_rate_hz), settings_(settings) {
    validate_estimator(sample_rate_hz, settings);
    const auto size = settings.segment_size;
    window_ = hann_window(size);
    double window_power_sum = 0.0;
    for (const double value : window_)
        window_power_sum += value * value;
    // One-sided density: |X|^2 / (fs * sum(w^2)), doubled for interior bins in process_segment.
    density_scale_ = 1.0 / (sample_rate_hz * window_power_sum);
    ring_.assign(size, 0.0);
    accumulator_.assign(size / 2 + 1, 0.0);
    work_.assign(size, std::complex<double>(0.0, 0.0));
}

void WelchPsd::push(std::span<const double> samples) {
    for (const double sample : samples)
        if (!std::isfinite(sample))
            throw std::invalid_argument("PSD samples must all be finite");
    const auto size = settings_.segment_size;
    for (const double sample : samples) {
        ring_[write_index_] = sample;
        write_index_ = write_index_ + 1 == size ? 0 : write_index_ + 1;
        ++samples_processed_;
        // The ring now holds exactly the last `size` samples, so a complete segment
        // starting at next_segment_start_ is available at this point.
        if (samples_processed_ == next_segment_start_ + size) {
            process_segment();
            next_segment_start_ += settings_.hop_size;
        }
    }
}

void WelchPsd::reset() noexcept {
    std::fill(ring_.begin(), ring_.end(), 0.0);
    std::fill(accumulator_.begin(), accumulator_.end(), 0.0);
    write_index_ = 0;
    samples_processed_ = 0;
    next_segment_start_ = 0;
    segments_ = 0;
}

void WelchPsd::process_segment() {
    const auto size = settings_.segment_size;
    // Oldest sample of the segment sits at write_index_.
    for (std::size_t i = 0; i < size; ++i) {
        std::size_t index = write_index_ + i;
        if (index >= size)
            index -= size;
        work_[i] = {ring_[index] * window_[i], 0.0};
    }
    detail::fft_radix2(work_);
    const std::size_t nyquist = size / 2;
    for (std::size_t bin = 0; bin <= nyquist; ++bin) {
        double density = std::norm(work_[bin]) * density_scale_;
        // DC and (for even sizes) Nyquist are not doubled in the one-sided density.
        if (bin != 0 && bin != nyquist)
            density *= 2.0;
        accumulator_[bin] += density;
    }
    ++segments_;
}

WelchResult WelchPsd::finish() const {
    const auto size = settings_.segment_size;
    WelchResult result;
    result.segment_size = size;
    result.hop_size = settings_.hop_size;
    result.bin_width_hz = sample_rate_hz_ / static_cast<double>(size);
    result.segments_averaged = segments_;
    result.samples_processed = samples_processed_;
    if (segments_ == 0)
        return result;
    const auto count = static_cast<double>(segments_);
    result.bins.reserve(size / 2 + 1);
    for (std::size_t bin = 0; bin <= size / 2; ++bin)
        result.bins.push_back({static_cast<double>(bin) * sample_rate_hz_ / static_cast<double>(size),
                               to_db(accumulator_[bin] / count)});
    return result;
}

WelchResult estimate_welch_psd(std::span<const double> samples, double sample_rate_hz,
                               WelchSettings settings) {
    WelchPsd estimator(sample_rate_hz, settings);
    estimator.push(samples);
    return estimator.finish();
}

PsdHarmonicCandidate measure_psd_harmonics(const WelchResult& psd, double nominal_hz,
                                           const PsdNoiseFloorSettings& settings) {
    validate_floor_settings(settings);
    if (!std::isfinite(nominal_hz) || nominal_hz <= 0.0)
        throw std::invalid_argument("nominal frequency must be finite and positive");
    if (psd.bins.size() < 3 || !std::isfinite(psd.bin_width_hz) || psd.bin_width_hz <= 0.0)
        throw std::invalid_argument("PSD has no interior bins to measure");

    const auto& bins = psd.bins;
    const std::size_t nyquist_index = bins.size() - 1;
    const std::size_t last_interior = bins.size() - 2;
    const double nyquist_hz = bins[nyquist_index].frequency_hz;
    PsdHarmonicCandidate candidate{nominal_hz, 0, {}};
    for (std::size_t harmonic = 1; harmonic <= settings.max_harmonics; ++harmonic) {
        const double expected = nominal_hz * static_cast<double>(harmonic);
        if (expected > nyquist_hz)
            break;
        const double position = expected / psd.bin_width_hz;
        const auto index = static_cast<std::size_t>(std::llround(position));
        if (index == 0 || index > nyquist_index)
            continue;

        // Search only the two bins bracketing the nominal frequency. A wider window reaches the
        // Hann main lobe of a nearby stronger tone (for example 60 Hz leaking into a 50 Hz search).
        const auto lower = static_cast<std::size_t>(std::floor(position));
        const auto upper = static_cast<std::size_t>(std::ceil(position));
        std::size_t peak = std::max<std::size_t>(lower, 1);
        for (std::size_t candidate_bin = peak; candidate_bin <= std::min(upper, nyquist_index);
             ++candidate_bin)
            if (bins[candidate_bin].density_dbfs2_per_hz > bins[peak].density_dbfs2_per_hz)
                peak = candidate_bin;

        // Median over interior bins within radius_bins, excluding the guard band around the harmonic.
        std::vector<double> neighbors;
        const auto center = static_cast<long long>(index);
        const auto radius = static_cast<long long>(settings.radius_bins);
        const auto guard = static_cast<long long>(settings.guard_bins);
        for (long long bin = center - radius; bin <= center + radius; ++bin) {
            if (bin < 1 || bin > static_cast<long long>(last_interior))
                continue;
            const auto distance = bin > center ? bin - center : center - bin;
            if (distance <= guard)
                continue;
            neighbors.push_back(bins[static_cast<std::size_t>(bin)].density_dbfs2_per_hz);
        }
        const double floor_db = neighbors.empty() ? negative_infinity : median_of(neighbors);
        const double level_db = bins[peak].density_dbfs2_per_hz;
        const double prominence = prominence_of(level_db, floor_db);
        const bool prominent = std::isfinite(level_db) && prominence >= settings.minimum_prominence_db;
        candidate.prominent_harmonics += prominent ? 1U : 0U;
        candidate.harmonics.push_back({harmonic, expected, bins[peak].frequency_hz, level_db,
                                       floor_db, prominence, prominent});
    }
    return candidate;
}

std::optional<PsdSummary> summarize_welch_psd(const WelchResult& psd,
                                              const PsdNoiseFloorSettings& settings) {
    validate_floor_settings(settings);
    if (psd.segments_averaged == 0 || psd.bins.size() < 3)
        return std::nullopt;
    std::vector<double> interior;
    interior.reserve(psd.bins.size() - 2);
    for (std::size_t bin = 1; bin + 1 < psd.bins.size(); ++bin)
        interior.push_back(psd.bins[bin].density_dbfs2_per_hz);

    PsdSummary summary;
    summary.segments_averaged = psd.segments_averaged;
    summary.samples_processed = psd.samples_processed;
    summary.bin_width_hz = psd.bin_width_hz;
    summary.broadband_median_dbfs2_per_hz = median_of(interior);
    summary.mains_50 = measure_psd_harmonics(psd, 50.0, settings);
    summary.mains_60 = measure_psd_harmonics(psd, 60.0, settings);
    return summary;
}

} // namespace humtrace
