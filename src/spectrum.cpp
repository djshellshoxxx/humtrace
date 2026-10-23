#include "humtrace/spectrum.hpp"
#include "fft.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace humtrace {
namespace {
[[nodiscard]] double to_dbfs(double amplitude) {
    return amplitude <= 0.0 ? -std::numeric_limits<double>::infinity()
                            : 20.0 * std::log10(amplitude);
}

[[nodiscard]] std::optional<double> local_spectral_floor(const std::vector<SpectrumBin>& spectrum,
                                          std::size_t center, const PeakSearchSettings& settings = {}) {
    const auto guard_bins = settings.floor_guard_bins;
    const auto radius_bins = settings.floor_radius_bins;
    std::vector<double> neighbors;
    const auto first = center > radius_bins ? center - radius_bins : 1;
    const auto last = std::min(spectrum.size() - 2, center + radius_bins);
    for (std::size_t index = first; index <= last; ++index) {
        const auto distance = index > center ? index - center : center - index;
        if (distance > guard_bins)
            neighbors.push_back(spectrum[index].level_dbfs);
    }
    if (neighbors.empty())
        return std::nullopt;
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

[[nodiscard]] SpectralPeak interpolate_peak(const std::vector<SpectrumBin>& spectrum, std::size_t index,
                                           const PeakSearchSettings& settings = {}) {
    const auto& bin = spectrum[index];
    SpectralPeak result{bin.frequency_hz, bin.level_dbfs};
    result.raw_bin_frequency_hz = bin.frequency_hz;
    result.bin_spacing_hz = spectrum[1].frequency_hz - spectrum[0].frequency_hz;
    const auto sample_count = 2.0 * static_cast<double>(spectrum.size() - 1);
    result.hann_main_lobe_width_hz = 4.0 * result.bin_spacing_hz * sample_count / (sample_count - 1.0);
    result.boundary_peak = index <= 1 || index + 1 >= spectrum.size() ||
        bin.frequency_hz <= settings.minimum_frequency_hz ||
        (settings.maximum_frequency_hz > 0 && bin.frequency_hz >= settings.maximum_frequency_hz);
    const auto floor = local_spectral_floor(spectrum, index, settings);
    result.local_floor_available = floor.has_value();
    result.local_floor_dbfs = floor.value_or(-std::numeric_limits<double>::infinity());
    result.prominence_db = floor ? bin.level_dbfs - *floor : 0.0;
    if (bin.level_dbfs == result.local_floor_dbfs && !std::isfinite(bin.level_dbfs)) result.prominence_db = 0.0;
    for (std::size_t other = index > 4 ? index - 4 : 1;
         other < std::min(spectrum.size() - 1, index + 5); ++other) {
        if (other == index) continue;
        if (spectrum[other].level_dbfs >= spectrum[other - 1].level_dbfs &&
            spectrum[other].level_dbfs >= spectrum[other + 1].level_dbfs &&
            spectrum[other].level_dbfs >= bin.level_dbfs - 10.0)
            result.competing_peak = true;
    }
    if (index == 0 || index + 1 >= spectrum.size()) return result;
    const double left = spectrum[index - 1].level_dbfs;
    const double center = bin.level_dbfs;
    const double right = spectrum[index + 1].level_dbfs;
    if (!std::isfinite(left) || !std::isfinite(center) || !std::isfinite(right))
        return result;
    if (center <= left || center <= right)
        return result;
    const double curvature = left - 2.0 * center + right;
    if (curvature >= 0.0)
        return result;
    const double offset = std::clamp(0.5 * (left - right) / curvature, -0.5, 0.5);
    const double bin_width = bin.frequency_hz - spectrum[index - 1].frequency_hz;
    result.frequency_hz = bin.frequency_hz + offset * bin_width;
    result.level_dbfs = center - 0.25 * (left - right) * offset;
    if (floor) result.prominence_db = result.level_dbfs - *floor;
    result.interpolation_applied = true;
    const double half_power_level = center - 10.0 * std::log10(2.0);
    std::size_t lower = index, upper = index;
    while (lower > 0 && spectrum[lower].level_dbfs > half_power_level) --lower;
    while (upper + 1 < spectrum.size() && spectrum[upper].level_dbfs > half_power_level) ++upper;
    if (lower < index && upper > index && std::isfinite(spectrum[lower].level_dbfs) &&
        std::isfinite(spectrum[upper].level_dbfs) && spectrum[lower].level_dbfs <= half_power_level &&
        spectrum[upper].level_dbfs <= half_power_level) {
        const auto crossing = [&](std::size_t a, std::size_t b) {
            return spectrum[a].frequency_hz + (half_power_level - spectrum[a].level_dbfs) /
                (spectrum[b].level_dbfs - spectrum[a].level_dbfs) *
                (spectrum[b].frequency_hz - spectrum[a].frequency_hz);
        };
        result.width_hz = crossing(upper - 1, upper) - crossing(lower, lower + 1);
    }
    return result;
}
} // namespace

void validate_peak_search_settings(const PeakSearchSettings& s) {
    if (!std::isfinite(s.minimum_frequency_hz) || s.minimum_frequency_hz < 0)
        throw std::invalid_argument("peak minimum frequency must be finite and non-negative");
    if (!std::isfinite(s.maximum_frequency_hz) || s.maximum_frequency_hz < 0 ||
        (s.maximum_frequency_hz != 0 && s.maximum_frequency_hz <= s.minimum_frequency_hz))
        throw std::invalid_argument("peak maximum frequency must be zero (Nyquist) or greater than minimum");
    if (!std::isfinite(s.minimum_separation_hz) || s.minimum_separation_hz < 0)
        throw std::invalid_argument("peak separation must be finite and non-negative");
    if (s.floor_radius_bins > (std::size_t{1} << 20) || s.floor_radius_bins <= s.floor_guard_bins)
        throw std::invalid_argument("peak floor radius must exceed guard bins and be at most 1048576");
}

std::vector<SpectrumBin> analyze_spectrum(const std::vector<double>& samples,
                                          double sample_rate_hz, bool remove_mean) {
    if (samples.size() < 4 || (samples.size() & (samples.size() - 1)) != 0)
        throw std::invalid_argument("sample count must be a power of two and at least 4");
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0)
        throw std::invalid_argument("sample rate must be finite and positive");

    const auto size = samples.size();
    std::vector<std::complex<double>> transformed(size);
    double window_sum = 0.0;
    double mean = 0.0;
    for (const double sample : samples) {
        if (!std::isfinite(sample))
            throw std::invalid_argument("samples must all be finite");
        mean += sample;
    }
    // Remove the frame mean first: a recording's DC offset would otherwise leak through the Hann
    // main lobe into bin 1 and masquerade as the strongest "non-DC" component.
    mean = remove_mean ? mean / static_cast<double>(size) : 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(size - 1));
        window_sum += window;
        transformed[i] = {(samples[i] - mean) * window, 0.0};
    }
    if (!std::isfinite(mean))
        throw std::invalid_argument("sample magnitudes are too large to analyze");
    detail::fft_radix2(transformed);
    for (const auto& value : transformed)
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag()))
            throw std::invalid_argument("sample magnitudes are too large to analyze");

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

SpectralPeak strongest_peak(const std::vector<SpectrumBin>& spectrum, const PeakSearchSettings& search) {
    validate_peak_search_settings(search);
    if (spectrum.size() < 2)
        throw std::invalid_argument("spectrum has no non-DC bins");
    const auto first = std::find_if(spectrum.begin() + 1, spectrum.end(), [&](const auto& bin) {
        return bin.frequency_hz >= search.minimum_frequency_hz;
    });
    const auto last = std::find_if(first, spectrum.end(), [&](const auto& bin) {
        return search.maximum_frequency_hz > 0 && bin.frequency_hz > search.maximum_frequency_hz;
    });
    if (first == last) return {0.0, -std::numeric_limits<double>::infinity()};
    const auto peak = std::max_element(first, last,
        [](const auto& lhs, const auto& rhs) { return lhs.level_dbfs < rhs.level_dbfs; });
    if (std::isinf(peak->level_dbfs) && peak->level_dbfs < 0.0)
        return {0.0, peak->level_dbfs};
    return interpolate_peak(spectrum, static_cast<std::size_t>(peak - spectrum.begin()), search);
}

std::vector<SpectralPeak> find_peaks(const std::vector<SpectrumBin>& spectrum,
                                     double min_level_dbfs, double min_prominence_db,
                                     std::size_t max_peaks, const PeakSearchSettings& search) {
    validate_peak_search_settings(search);
    if (spectrum.size() < 4 || !std::isfinite(min_level_dbfs) || !std::isfinite(min_prominence_db) ||
        min_prominence_db < 0.0 || max_peaks == 0)
        throw std::invalid_argument("invalid peak search settings");
    struct Candidate { std::size_t index; double level; };
    std::vector<Candidate> found;
    for (std::size_t i = 1; i + 1 < spectrum.size(); ++i) {
        const double level = spectrum[i].level_dbfs;
        if (spectrum[i].frequency_hz < search.minimum_frequency_hz ||
            (search.maximum_frequency_hz > 0 && spectrum[i].frequency_hz > search.maximum_frequency_hz)) continue;
        if (!std::isfinite(level) || level < min_level_dbfs ||
            level < spectrum[i - 1].level_dbfs || level <= spectrum[i + 1].level_dbfs)
            continue;
        const auto floor = local_spectral_floor(spectrum, i, search);
        if (!floor || (std::isfinite(*floor) && level - *floor < min_prominence_db))
            continue;
        found.push_back({i, level});
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const auto& l, const auto& r) { return l.level > r.level; });
    std::vector<Candidate> separated;
    for (const auto& candidate : found) {
        const bool close = std::any_of(separated.begin(), separated.end(), [&](const auto& kept) {
            return std::abs(spectrum[candidate.index].frequency_hz - spectrum[kept.index].frequency_hz) < search.minimum_separation_hz;
        });
        if (!close) separated.push_back(candidate);
        if (separated.size() == max_peaks) break;
    }
    found = std::move(separated);
    std::sort(found.begin(), found.end(),
              [](const auto& l, const auto& r) { return l.index < r.index; });
    std::vector<SpectralPeak> peaks;
    peaks.reserve(found.size());
    for (const auto& c : found)
        peaks.push_back(interpolate_peak(spectrum, c.index, search));
    return peaks;
}

HarmonicCandidate measure_harmonics(const std::vector<SpectrumBin>& spectrum,
                                    double nominal_hz, double support_threshold_dbfs,
                                    std::size_t max_harmonics,
                                    double minimum_prominence_db, double harmonic_tolerance_hz,
                                    const PeakSearchSettings& search) {
    validate_peak_search_settings(search);
    if (spectrum.size() < 2 || !std::isfinite(nominal_hz) || nominal_hz <= 0.0 ||
        !std::isfinite(support_threshold_dbfs) || max_harmonics == 0 ||
        !std::isfinite(minimum_prominence_db) || minimum_prominence_db < 0.0 ||
        !std::isfinite(harmonic_tolerance_hz) || harmonic_tolerance_hz < 0)
        throw std::invalid_argument("invalid spectrum or harmonic measurement settings");
    const double spacing = spectrum[1].frequency_hz - spectrum[0].frequency_hz;
    if (!std::isfinite(spacing) || spacing <= 0.0)
        throw std::invalid_argument("spectrum bins must have increasing frequencies");
    HarmonicCandidate candidate{nominal_hz, 0, {}};
    // Harmonics are only measurable when the bin spacing resolves them: with coarser bins several
    // harmonics (and DC) collapse onto one bin and would produce fabricated "support".
    if (spacing > nominal_hz / 2.0)
        return candidate;
    candidate.harmonics.reserve(std::min(max_harmonics, spectrum.size()));
    const double nyquist_hz = spectrum.back().frequency_hz;
    for (std::size_t harmonic = 1; harmonic <= max_harmonics; ++harmonic) {
        const double expected = nominal_hz * static_cast<double>(harmonic);
        if (expected > nyquist_hz)
            break;
        auto index = static_cast<std::size_t>(std::llround(expected / spacing));
        if (index >= spectrum.size())
            break;
        if (index == 0)
            continue; // never treat DC as a harmonic
        if (expected < search.minimum_frequency_hz || (search.maximum_frequency_hz > 0 && expected > search.maximum_frequency_hz)) continue;
        const double tolerance = harmonic_tolerance_hz == 0 ? spacing / 2.0 :
            std::min(std::max(harmonic_tolerance_hz, spacing), std::nextafter(nominal_hz / 2.0, 0.0));
        bool local_maximum = true;
        if (harmonic_tolerance_hz > 0) {
            std::optional<std::size_t> best;
            const auto first = static_cast<std::size_t>(std::max(1.0, std::ceil((expected - tolerance) / spacing)));
            const auto last = static_cast<std::size_t>(std::min(static_cast<double>(spectrum.size() - 2), std::floor((expected + tolerance) / spacing)));
            for (std::size_t bin = first; bin <= last; ++bin) {
                if (spectrum[bin].frequency_hz < search.minimum_frequency_hz ||
                    (search.maximum_frequency_hz > 0 && spectrum[bin].frequency_hz > search.maximum_frequency_hz)) continue;
                if (spectrum[bin].level_dbfs <= spectrum[bin - 1].level_dbfs ||
                    spectrum[bin].level_dbfs <= spectrum[bin + 1].level_dbfs) continue;
                if (!best || spectrum[bin].level_dbfs > spectrum[*best].level_dbfs) best = bin;
            }
            local_maximum = best.has_value();
            if (best) index = *best;
        }
        const auto& bin = spectrum[index];
        const auto measured_floor = local_spectral_floor(spectrum, index, search);
        const double floor = measured_floor.value_or(-std::numeric_limits<double>::infinity());
        double prominence = bin.level_dbfs - floor;
        if (std::isinf(floor) && floor < 0.0 && std::isfinite(bin.level_dbfs))
            prominence = std::numeric_limits<double>::infinity();
        if (std::isinf(floor) && floor < 0.0 && std::isinf(bin.level_dbfs) && bin.level_dbfs < 0.0)
            prominence = 0.0;
        const auto refined = interpolate_peak(spectrum, index, search);
        const bool in_search = harmonic_tolerance_hz == 0 || std::abs(refined.frequency_hz - expected) <= tolerance;
        const bool supported = in_search && local_maximum && measured_floor.has_value() && bin.level_dbfs >= support_threshold_dbfs &&
                               prominence >= minimum_prominence_db;
        candidate.supporting_harmonics += supported ? 1U : 0U;
        candidate.harmonics.push_back({harmonic, expected, bin.frequency_hz,
                                       bin.level_dbfs, floor, prominence, supported,
                                       refined.frequency_hz, tolerance,
                                       measured_floor.has_value(), index <= 1 || index + 1 >= spectrum.size()});
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

InterferenceFrame measure_frame(const std::vector<SpectrumBin>& spectrum, std::uint64_t start_sample,
                                double support_threshold_dbfs, std::size_t max_harmonics,
                                double minimum_prominence_db, double harmonic_tolerance_hz,
                                const PeakSearchSettings& search) {
    return {start_sample, strongest_peak(spectrum, search),
            measure_harmonics(spectrum, 50.0, support_threshold_dbfs, max_harmonics, minimum_prominence_db, harmonic_tolerance_hz, search),
            measure_harmonics(spectrum, 60.0, support_threshold_dbfs, max_harmonics, minimum_prominence_db, harmonic_tolerance_hz, search)};
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
        result.push_back(measure_frame(analyze_spectrum(block, sample_rate_hz),
            static_cast<std::uint64_t>(start), support_threshold_dbfs, max_harmonics, minimum_prominence_db));
        result.back().levels = measure_sample_levels(block);
    }
    return result;
}

} // namespace humtrace
