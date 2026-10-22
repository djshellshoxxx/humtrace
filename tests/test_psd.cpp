#include "humtrace/psd.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

template <typename Callable>
bool throws_invalid_argument(Callable&& callable) {
    try {
        callable();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

// Portable deterministic generator; std distributions are not bit-reproducible across standard libraries.
class SplitMix64 {
public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
    // Uniform in [-0.5, 0.5), variance 1/12.
    double uniform_centered() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z ^= z >> 31;
        return static_cast<double>(z >> 11) * 0x1.0p-53 - 0.5;
    }

private:
    std::uint64_t state_;
};

std::vector<double> uniform_noise(std::size_t count, std::uint64_t seed) {
    SplitMix64 generator(seed);
    std::vector<double> samples(count);
    for (auto& sample : samples)
        sample = generator.uniform_centered();
    return samples;
}

// Same symmetric Hann definition as the implementation, computed independently here.
std::vector<double> hann(std::size_t size) {
    std::vector<double> window(size);
    for (std::size_t i = 0; i < size; ++i)
        window[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                         static_cast<double>(size - 1));
    return window;
}

double linear_density(const humtrace::PsdBin& bin) {
    return std::pow(10.0, bin.density_dbfs2_per_hz / 10.0);
}

// Mean and relative standard deviation of linear density over interior bins.
struct DensityStats {
    double mean{};
    double relative_std{};
};

DensityStats interior_stats(const humtrace::WelchResult& result) {
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t bin = 1; bin + 1 < result.bins.size(); ++bin) {
        sum += linear_density(result.bins[bin]);
        ++count;
    }
    const double mean = sum / static_cast<double>(count);
    double variance = 0.0;
    for (std::size_t bin = 1; bin + 1 < result.bins.size(); ++bin) {
        const double deviation = linear_density(result.bins[bin]) - mean;
        variance += deviation * deviation;
    }
    variance /= static_cast<double>(count);
    return {mean, std::sqrt(variance) / mean};
}

bool identical(const humtrace::WelchResult& lhs, const humtrace::WelchResult& rhs) {
    if (lhs.segments_averaged != rhs.segments_averaged ||
        lhs.samples_processed != rhs.samples_processed || lhs.bins.size() != rhs.bins.size())
        return false;
    for (std::size_t bin = 0; bin < lhs.bins.size(); ++bin)
        if (lhs.bins[bin].frequency_hz != rhs.bins[bin].frequency_hz ||
            lhs.bins[bin].density_dbfs2_per_hz != rhs.bins[bin].density_dbfs2_per_hz)
            return false;
    return true;
}

void white_noise_density_matches_one_sided_analytic_level() {
    constexpr double sample_rate = 8000.0;
    const auto samples = uniform_noise(65536, 1);
    const auto result = humtrace::estimate_welch_psd(samples, sample_rate, {1024, 512});
    require(result.segments_averaged == 127, "65536 samples at hop 512 give 127 complete segments");
    // Variance 1/12 spread over 0..fs/2 gives one-sided density 2*sigma^2/fs.
    const double expected = 2.0 * (1.0 / 12.0) / sample_rate;
    const auto stats = interior_stats(result);
    require(std::abs(stats.mean / expected - 1.0) < 0.03,
            "white-noise mean density is within 3% of 2*sigma^2/fs");
    require(std::abs(10.0 * std::log10(stats.mean / expected)) < 0.15,
            "white-noise mean density is within 0.15 dB of the analytic level");
}

void parseval_scaling_is_exact_for_one_segment() {
    // Sum of the one-sided density times bin width equals sum((x*w)^2) / sum(w^2) exactly,
    // which fails if DC or Nyquist are doubled or the density scale is wrong.
    constexpr std::size_t size = 256;
    constexpr double sample_rate = 1000.0;
    const auto samples = uniform_noise(size, 7);
    const auto result = humtrace::estimate_welch_psd(samples, sample_rate, {size, size});
    require(result.segments_averaged == 1, "one full segment is averaged");

    const auto window = hann(size);
    double window_power = 0.0;
    double weighted_square_sum = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        window_power += window[i] * window[i];
        const double value = samples[i] * window[i];
        weighted_square_sum += value * value;
    }
    double integrated = 0.0;
    for (const auto& bin : result.bins)
        integrated += linear_density(bin) * (sample_rate / static_cast<double>(size));
    const double expected = weighted_square_sum / window_power;
    require(std::abs(integrated / expected - 1.0) < 1e-9,
            "integrated one-sided PSD equals the windowed mean square (Parseval)");
}

void tone_peak_is_at_the_right_bin_with_analytic_level() {
    constexpr double sample_rate = 8000.0;
    constexpr std::size_t size = 1024;
    constexpr std::size_t tone_bin = 64; // 500 Hz exactly, bin-centred
    constexpr double amplitude = 0.25;
    std::vector<double> samples(16384);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = amplitude * std::cos(2.0 * std::numbers::pi * static_cast<double>(tone_bin) *
                                          static_cast<double>(i) / static_cast<double>(size));
    const auto result = humtrace::estimate_welch_psd(samples, sample_rate, {size, size / 2});

    std::size_t peak = 0;
    for (std::size_t bin = 1; bin < result.bins.size(); ++bin)
        if (result.bins[bin].density_dbfs2_per_hz > result.bins[peak].density_dbfs2_per_hz)
            peak = bin;
    require(peak == tone_bin, "tone peak is in the expected bin");
    require(std::abs(result.bins[peak].frequency_hz - 500.0) < 1e-9, "tone peak frequency is 500 Hz");

    // Peak density of a bin-centred cosine: A^2 C^2 / (2 fs S2), with C = sum(w) and S2 = sum(w^2).
    const auto window = hann(size);
    double coherent_gain = 0.0;
    double power = 0.0;
    for (const double value : window) {
        coherent_gain += value;
        power += value * value;
    }
    const double expected_db = 10.0 * std::log10(amplitude * amplitude * coherent_gain * coherent_gain /
                                                 (2.0 * sample_rate * power));
    require(std::abs(result.bins[peak].density_dbfs2_per_hz - expected_db) < 0.05,
            "tone peak level matches the analytic PSD within 0.05 dB");
    require(result.bins[peak].density_dbfs2_per_hz - result.bins[tone_bin + 5].density_dbfs2_per_hz > 40.0,
            "Hann leakage falls well below the peak five bins away");
}

void welch_averaging_reduces_variance_relative_to_one_periodogram() {
    const auto single = humtrace::estimate_welch_psd(uniform_noise(1024, 3), 4096.0, {1024, 512});
    const auto averaged = humtrace::estimate_welch_psd(uniform_noise(16384, 3), 4096.0, {1024, 512});
    require(single.segments_averaged == 1 && averaged.segments_averaged == 31,
            "segment counts for the variance comparison are as expected");
    const auto single_stats = interior_stats(single);
    const auto averaged_stats = interior_stats(averaged);
    // A single Hann periodogram of noise has relative spread near 1 across bins.
    require(single_stats.relative_std > 0.8, "single periodogram shows the expected spread");
    require(averaged_stats.relative_std < 0.5 * single_stats.relative_std,
            "31-segment Welch average at least halves the relative spread");
}

void streaming_chunks_match_one_shot_estimate() {
    const auto samples = uniform_noise(20003, 11);
    const humtrace::WelchSettings settings{512, 200};
    const auto one_shot = humtrace::estimate_welch_psd(samples, 8000.0, settings);
    require(one_shot.segments_averaged == 1 + (samples.size() - 512) / 200,
            "segment count is 1 + (length - segment) / hop");
    require(one_shot.samples_processed == samples.size(), "all samples are counted");

    for (const std::size_t chunk : {std::size_t{1}, std::size_t{3}, std::size_t{199}, std::size_t{512},
                                    std::size_t{4096}, samples.size()}) {
        humtrace::WelchPsd estimator(8000.0, settings);
        for (std::size_t start = 0; start < samples.size(); start += chunk) {
            const auto count = std::min(chunk, samples.size() - start);
            estimator.push(std::span<const double>(samples).subspan(start, count));
        }
        require(identical(estimator.finish(), one_shot), "chunked streaming equals one-shot exactly");
    }
}

void identical_inputs_give_identical_results_and_reset_restores_state() {
    const auto samples = uniform_noise(4096, 5);
    const auto first = humtrace::estimate_welch_psd(samples, 4000.0, {256, 128});
    const auto second = humtrace::estimate_welch_psd(samples, 4000.0, {256, 128});
    require(identical(first, second), "repeated runs are bit-identical");

    humtrace::WelchPsd estimator(4000.0, {256, 128});
    estimator.push(uniform_noise(1000, 99));
    estimator.reset();
    require(estimator.segments_averaged() == 0 && estimator.samples_processed() == 0,
            "reset clears segments and samples");
    estimator.push(samples);
    require(identical(estimator.finish(), first), "reset followed by the same input matches a fresh estimator");
}

void partial_trailing_segment_is_never_included() {
    const auto result = humtrace::estimate_welch_psd(std::vector<double>(1000, 0.25), 1000.0, {512, 512});
    require(result.segments_averaged == 1, "a 488-sample tail does not form a second segment");
    require(result.samples_processed == 1000, "processed samples still count the tail");
    const auto minimal = humtrace::estimate_welch_psd(std::vector<double>(8, 0.5), 100.0, {4, 2});
    require(minimal.segments_averaged == 3 && minimal.bins.size() == 3,
            "smallest segment with 50% overlap yields three segments and three bins");
}

void validates_estimator_inputs() {
    require(throws_invalid_argument([] { (void)humtrace::WelchPsd(0.0, {}); }),
            "zero sample rate is rejected");
    require(throws_invalid_argument([] {
                (void)humtrace::WelchPsd(std::numeric_limits<double>::quiet_NaN(), {});
            }),
            "NaN sample rate is rejected");
    require(throws_invalid_argument([] { (void)humtrace::WelchPsd(-8000.0, {}); }),
            "negative sample rate is rejected");
    require(throws_invalid_argument([] { (void)humtrace::WelchPsd(8000.0, {.segment_size = 3, .hop_size = 1}); }),
            "non-power-of-two segment is rejected");
    require(throws_invalid_argument([] { (void)humtrace::WelchPsd(8000.0, {.segment_size = 2, .hop_size = 1}); }),
            "segment below 4 is rejected");
    require(throws_invalid_argument([] {
                (void)humtrace::WelchPsd(8000.0, {.segment_size = humtrace::max_welch_segment_size * 2, .hop_size = 1});
            }),
            "segment above the maximum is rejected");
    require(throws_invalid_argument([] { (void)humtrace::WelchPsd(8000.0, {.segment_size = 64, .hop_size = 0}); }),
            "zero hop is rejected");
    require(throws_invalid_argument([] { (void)humtrace::WelchPsd(8000.0, {.segment_size = 64, .hop_size = 65}); }),
            "hop larger than the segment is rejected");
}

void rejects_non_finite_samples_without_consuming_them() {
    humtrace::WelchPsd estimator(8000.0, {64, 32});
    estimator.push(std::vector<double>(100, 0.1));
    const auto consumed = estimator.samples_processed();
    const auto segments = estimator.segments_averaged();
    const std::vector<double> bad = {0.1, 0.2, std::numeric_limits<double>::quiet_NaN()};
    require(throws_invalid_argument([&] { estimator.push(bad); }), "NaN sample is rejected");
    const std::vector<double> infinite = {std::numeric_limits<double>::infinity()};
    require(throws_invalid_argument([&] { estimator.push(infinite); }), "infinite sample is rejected");
    require(estimator.samples_processed() == consumed && estimator.segments_averaged() == segments,
            "rejected input leaves the estimator unchanged");
    estimator.push(std::vector<double>(28, 0.1));
    require(estimator.segments_averaged() == segments + 1, "estimator continues after a rejected push");
}

void noise_floor_and_harmonic_measurements_separate_tone_from_noise() {
    constexpr double sample_rate = 4096.0;
    constexpr double noise_density = 2.0 * (1.0 / 12.0) / sample_rate;
    constexpr double tone_amplitude = 0.2;
    auto samples = uniform_noise(32768, 21);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] += tone_amplitude * std::sin(2.0 * std::numbers::pi * 60.0 *
                                                static_cast<double>(i) / sample_rate);
    const auto psd = humtrace::estimate_welch_psd(samples, sample_rate, {1024, 512});
    const humtrace::PsdNoiseFloorSettings floor_settings;
    const auto summary = humtrace::summarize_welch_psd(psd, floor_settings);
    require(summary.has_value(), "summary exists when segments were averaged");
    require(std::abs(summary->bin_width_hz - 4.0) < 1e-12, "bin width is fs/N");
    require(std::abs(summary->broadband_median_dbfs2_per_hz -
                     10.0 * std::log10(noise_density)) < 1.5,
            "broadband median tracks the analytic noise density");

    const auto& harmonic60 = summary->mains_60.harmonics.front();
    require(harmonic60.prominent && harmonic60.prominence_db >= 15.0,
            "60 Hz tone is prominent over the local median floor");
    require(std::abs(harmonic60.measured_bin_hz - 60.0) < 1e-9, "60 Hz measurement is at the tone bin");
    require(std::abs(harmonic60.local_floor_dbfs2_per_hz - 10.0 * std::log10(noise_density)) < 1.5,
            "local floor is the median noise density, not inflated by the tone");
    require(!summary->mains_50.harmonics.front().prominent,
            "absent 50 Hz fundamental is not prominent");
    require(!summary->mains_60.harmonics[1].prominent, "absent 120 Hz partial is not prominent");
    require(summary->mains_60.harmonics.size() == 10 && summary->mains_50.harmonics.size() == 10,
            "both nominal series report up to ten harmonics below Nyquist");
    require(summary->mains_60.prominent_harmonics == 1, "exactly one 60 Hz harmonic is prominent");
}

void noise_only_reports_no_prominent_harmonics() {
    for (const std::uint64_t seed : {std::uint64_t{31}, std::uint64_t{32}}) {
        const auto psd = humtrace::estimate_welch_psd(uniform_noise(32768, seed), 4096.0, {1024, 512});
        const auto summary = humtrace::summarize_welch_psd(psd, {});
        require(summary.has_value(), "noise-only summary exists");
        require(summary->mains_50.prominent_harmonics == 0 && summary->mains_60.prominent_harmonics == 0,
                "white noise alone yields no prominent 50/60 Hz harmonics");
    }
}

void harmonic_and_summary_inputs_are_validated() {
    const auto psd = humtrace::estimate_welch_psd(uniform_noise(4096, 2), 4096.0, {1024, 512});
    humtrace::PsdNoiseFloorSettings bad;
    bad.max_harmonics = 0;
    require(throws_invalid_argument([&] { (void)humtrace::measure_psd_harmonics(psd, 50.0, bad); }),
            "zero harmonic count is rejected");
    bad = {};
    bad.radius_bins = bad.guard_bins;
    require(throws_invalid_argument([&] { (void)humtrace::measure_psd_harmonics(psd, 50.0, bad); }),
            "a floor radius not larger than the guard is rejected");
    bad = {};
    bad.minimum_prominence_db = -1.0;
    require(throws_invalid_argument([&] { (void)humtrace::summarize_welch_psd(psd, bad); }),
            "negative prominence threshold is rejected");
    require(throws_invalid_argument([&] { (void)humtrace::measure_psd_harmonics(psd, 0.0, {}); }),
            "non-positive nominal frequency is rejected");

    const humtrace::WelchResult empty;
    require(!humtrace::summarize_welch_psd(empty, {}).has_value(),
            "a PSD without complete segments has no summary");
    const auto short_input = humtrace::estimate_welch_psd(std::vector<double>(100, 0.0), 4096.0, {1024, 512});
    require(!humtrace::summarize_welch_psd(short_input, {}).has_value(),
            "input shorter than one segment has no summary");
}

void silence_has_no_prominent_harmonics() {
    const auto psd = humtrace::estimate_welch_psd(std::vector<double>(4096, 0.0), 4096.0, {1024, 512});
    const auto summary = humtrace::summarize_welch_psd(psd, {});
    require(summary.has_value(), "silent input still has a summary");
    require(summary->mains_50.prominent_harmonics == 0 && summary->mains_60.prominent_harmonics == 0,
            "silence is not reported as prominent (no finite level)");
    require(std::isinf(summary->broadband_median_dbfs2_per_hz) && summary->broadband_median_dbfs2_per_hz < 0.0,
            "silent broadband median is -infinity");
}
} // namespace

int main() {
    { // Coarse resolution must not count the same broad bin as multiple mains harmonics.
        const auto psd = humtrace::estimate_welch_psd(std::vector<double>(64, 1.0), 48000, {64, 32});
        const auto candidate = humtrace::measure_psd_harmonics(psd, 50, {});
        require(candidate.prominent_harmonics == 0 && candidate.harmonics.empty(),
                "unresolved mains harmonics are not measured");
    }
    { // An empty local neighborhood is unknown evidence, not an infinitely quiet floor.
        humtrace::WelchResult psd;
        psd.bin_width_hz = 25;
        psd.bins = {{0, -100}, {25, -100}, {50, -10}, {75, -100}};
        const auto candidate = humtrace::measure_psd_harmonics(psd, 50, {});
        require(candidate.prominent_harmonics == 0, "missing noise floor cannot establish prominence");
    }
    {
        bool rejected = false;
        try { (void)humtrace::estimate_welch_psd(std::vector<double>(16, 1e200), 4096, {16, 8}); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "overflowing power is rejected rather than exported as missing data");
    }
    white_noise_density_matches_one_sided_analytic_level();
    parseval_scaling_is_exact_for_one_segment();
    tone_peak_is_at_the_right_bin_with_analytic_level();
    welch_averaging_reduces_variance_relative_to_one_periodogram();
    streaming_chunks_match_one_shot_estimate();
    identical_inputs_give_identical_results_and_reset_restores_state();
    partial_trailing_segment_is_never_included();
    validates_estimator_inputs();
    rejects_non_finite_samples_without_consuming_them();
    noise_floor_and_harmonic_measurements_separate_tone_from_noise();
    noise_only_reports_no_prominent_harmonics();
    harmonic_and_summary_inputs_are_validated();
    silence_has_no_prominent_harmonics();
    std::cout << "All PSD tests passed.\n";
}
