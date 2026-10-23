#include "humtrace/spectrum.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <type_traits>
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

void harmonic_support_requires_prominence_over_local_noise() {
    constexpr double sample_rate = 4096.0;
    constexpr std::size_t count = 4096;
    std::vector<double> samples(count);
    std::uint32_t state = 0x12345678U;
    for (std::size_t i = 0; i < count; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        const double noise = (static_cast<double>(state) / 4294967295.0 - 0.5) * 0.02;
        const double time = static_cast<double>(i) / sample_rate;
        samples[i] = noise + 0.25 * std::sin(2.0 * std::numbers::pi * 60.0 * time);
    }
    const auto spectrum = humtrace::analyze_spectrum(samples, sample_rate);
    const auto candidate = humtrace::measure_harmonics(spectrum, 60.0, -80.0, 5, 15.0);
    require(candidate.harmonics[0].prominence_db > 20.0,
            "clear sinusoid is measured above its local spectral floor");
    require(candidate.harmonics[0].supports_threshold,
            "a tone above both absolute and local-prominence thresholds is supported");
    require(!candidate.harmonics[1].supports_threshold,
            "noise-only harmonic is not supported by an absolute cutoff alone");
}

void interpolates_frequency_between_fft_bins() {
    constexpr double sample_rate = 4096.0;
    constexpr std::size_t count = 4096;
    constexpr double frequency = 60.4;
    std::vector<double> samples(count);
    for (std::size_t i = 0; i < count; ++i)
        samples[i] = 0.5 * std::sin(2.0 * std::numbers::pi * frequency *
                                    static_cast<double>(i) / sample_rate);
    const auto peak = humtrace::strongest_peak(humtrace::analyze_spectrum(samples, sample_rate));
    require(std::abs(peak.frequency_hz - frequency) < 0.1,
            "parabolic peak interpolation estimates an off-bin tone");
    require(std::abs(peak.level_dbfs - (-6.0206)) < 0.25,
            "off-bin peak level is corrected for scalloping");
}
void finds_multiple_prominent_peaks() {
    constexpr double rate = 4096.0;
    std::vector<double> samples(4096);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto t = static_cast<double>(i) / rate;
        samples[i] = 0.5 * std::sin(2.0 * std::numbers::pi * 60.0 * t) +
                     0.25 * std::sin(2.0 * std::numbers::pi * 180.0 * t);
    }
    const auto spectrum = humtrace::analyze_spectrum(samples, rate);
    const auto peaks = humtrace::find_peaks(spectrum, -60.0, 10.0, 8);
    require(peaks.size() == 2, "exactly the two tones are reported");
    require(std::abs(peaks[0].frequency_hz - 60.0) < 0.2 && std::abs(peaks[1].frequency_hz - 180.0) < 0.2,
            "peaks are in ascending frequency");
    require(humtrace::find_peaks(spectrum, -60.0, 10.0, 1).size() == 1, "max_peaks is honored");
    bool threw = false;
    try { (void)humtrace::find_peaks(spectrum, -60.0, -1.0, 1); } catch (const std::invalid_argument&) { threw = true; }
    require(threw, "negative prominence is rejected");
}
void dc_offset_does_not_masquerade_as_the_strongest_component() {
    constexpr double rate = 8000.0;
    std::vector<double> samples(4096);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = 0.1 + 0.01 * std::sin(2.0 * std::numbers::pi * 50.0 * static_cast<double>(i) / rate);
    const auto spectrum = humtrace::analyze_spectrum(samples, rate);
    const auto peak = humtrace::strongest_peak(spectrum);
    require(std::abs(peak.frequency_hz - 50.0) < 0.5, "50 Hz hum is strongest despite a DC offset");
    for (const auto& p : humtrace::find_peaks(spectrum, -80.0, 10.0, 8))
        require(p.frequency_hz > 10.0, "no leaked-DC peak near 0 Hz");
}
void unresolvable_harmonics_yield_no_evidence_and_huge_samples_are_rejected() {
    // 64-sample block at 48 kHz: 750 Hz bins cannot resolve 50/60 Hz harmonics.
    std::vector<double> samples(64);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = 0.5 * std::sin(2.0 * std::numbers::pi * 1500.0 * static_cast<double>(i) / 48000.0);
    const auto spectrum = humtrace::analyze_spectrum(samples, 48000.0);
    const auto candidate = humtrace::measure_harmonics(spectrum, 50.0, -60.0, 10);
    require(candidate.harmonics.empty() && candidate.supporting_harmonics == 0, "no fabricated harmonic support at coarse resolution");
    std::vector<double> huge(64, 1e308);
    huge[1] = -1e308;
    bool threw = false;
    try { (void)humtrace::analyze_spectrum(huge, 48000.0); } catch (const std::invalid_argument&) { threw = true; }
    require(threw, "overflowing sample magnitudes are rejected, not turned into NaN peaks");
}
void absent_floor_does_not_establish_peak_or_harmonic_prominence() {
    const std::vector<humtrace::SpectrumBin> spectrum{{0, -100}, {25, -100}, {50, -10}, {75, -100}};
    require(humtrace::find_peaks(spectrum, -60, 10, 8).empty(),
            "an empty floor neighborhood cannot establish peak prominence");
    require(humtrace::measure_harmonics(spectrum, 50, -60, 1).supporting_harmonics == 0,
            "an empty floor neighborhood cannot establish harmonic prominence");
}

void a_flat_maximum_is_not_interpolated() {
    const std::vector<humtrace::SpectrumBin> spectrum{{0, -100}, {1, -20}, {2, -10}, {3, -10}, {4, -30}};
    const auto peak = humtrace::strongest_peak(spectrum);
    require(peak.frequency_hz == 2.0, "flat peaks retain the raw bin instead of an invented refinement");
}

void peak_metadata_distinguishes_resolution_from_uncertainty() {
    std::vector<double> samples(4096);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = 0.5 * std::sin(2 * std::numbers::pi * 60.4 * static_cast<double>(i) / 4096.0);
    const auto peak = humtrace::strongest_peak(humtrace::analyze_spectrum(samples, 4096));
    require(peak.raw_bin_frequency_hz == 60 && peak.bin_spacing_hz == 1,
            "raw frequency and FFT bin spacing survive interpolation");
    require(peak.interpolation_applied && std::abs(peak.frequency_hz - 60.4) < 0.1,
            "an applied valid refinement is identified");
    require(peak.hann_main_lobe_width_hz > 4 && peak.hann_main_lobe_width_hz < 4.01,
            "symmetric Hann null-to-null main lobe is about four bin spacings");
    require(!peak.frequency_uncertainty_hz.has_value(),
            "bin spacing and interpolation do not fabricate calibrated uncertainty");
    require(peak.width_hz && *peak.width_hz > 1 && *peak.width_hz < 2,
            "a measured half-power width is separate from uncertainty");
    require(peak.local_floor_available && peak.prominence_db > 20,
            "peak retains its local spectral floor evidence");
    require(std::abs(peak.prominence_db - (peak.level_dbfs - peak.local_floor_dbfs)) < 1e-12,
            "exported prominence uses the same corrected peak level as the measurement");
    static_assert(std::is_trivially_copyable_v<humtrace::SpectralPeak>);
}

void configurable_peak_search_honors_band_separation_and_floor() {
    std::vector<humtrace::SpectrumBin> bins;
    for (int i = 0; i <= 100; ++i) bins.push_back({static_cast<double>(i), -100});
    bins[20].level_dbfs = -10;
    bins[25].level_dbfs = -20;
    bins[70].level_dbfs = -30;
    humtrace::PeakSearchSettings search;
    search.minimum_frequency_hz = 10;
    search.maximum_frequency_hz = 50;
    search.minimum_separation_hz = 6;
    const auto peaks = humtrace::find_peaks(bins, -60, 10, 8, search);
    require(peaks.size() == 1 && peaks[0].raw_bin_frequency_hz == 20,
            "band excludes distant tones and separation retains the strongest nearby peak");
    search.minimum_frequency_hz = 50;
    search.maximum_frequency_hz = 90;
    require(humtrace::strongest_peak(bins, search).raw_bin_frequency_hz == 70,
            "strongest-peak measurement honors the same configured band");
    search.minimum_frequency_hz = 10;
    search.maximum_frequency_hz = 50;
    search.minimum_separation_hz = 0;
    require(humtrace::find_peaks(bins, -60, 10, 8, search).size() == 2,
            "zero separation retains independently resolvable local maxima");
    search.floor_guard_bins = 0;
    search.floor_radius_bins = 1;
    bins[19].level_dbfs = bins[21].level_dbfs = -15;
    require(humtrace::find_peaks(bins, -60, 10, 8, search).size() == 1,
            "configured immediate-neighbor floor rejects a low-prominence peak");
}

void configured_harmonic_search_measures_off_nominal_local_maxima() {
    std::vector<double> samples(4096);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto t = static_cast<double>(i) / 4096;
        samples[i] = 0.4 * std::sin(2 * std::numbers::pi * 63 * t) +
                     0.2 * std::sin(2 * std::numbers::pi * 126 * t);
    }
    const auto spectrum = humtrace::analyze_spectrum(samples, 4096);
    const auto legacy = humtrace::measure_harmonics(spectrum, 60, -40, 2);
    const auto searched = humtrace::measure_harmonics(spectrum, 60, -40, 2, 10, 7);
    require(legacy.supporting_harmonics == 0 && searched.supporting_harmonics == 2,
            "opt-in bounded search recovers the off-nominal family missed at nominal bins");
    require(searched.harmonics[0].measured_bin_hz == 63 && searched.harmonics[1].measured_bin_hz == 126,
            "observed partials retain their measured bins instead of nominal labels");
    require(searched.harmonics[1].search_tolerance_hz == 7,
            "the actual bounded search tolerance is exposed");
    const auto clamped = humtrace::measure_harmonics(spectrum, 60, -40, 2, 10, 100);
    require(clamped.harmonics[0].search_tolerance_hz < 30,
            "search regions are clamped before adjacent harmonic regions overlap");
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = 0.4 * std::sin(2 * std::numbers::pi * 63.4 * static_cast<double>(i) / 4096.0);
    const auto beyond_bound = humtrace::measure_harmonics(humtrace::analyze_spectrum(samples, 4096), 60, -40, 1, 10, 3);
    require(beyond_bound.harmonics[0].measured_bin_hz == 63 && beyond_bound.harmonics[0].frequency_hz > 63 &&
            !beyond_bound.harmonics[0].supports_threshold,
            "refinement beyond the configured search bound cannot establish harmonic support");
}

void spectrum_detrending_is_explicit_and_preserves_the_input() {
    const std::vector<double> samples(64, 0.25);
    const auto ac = humtrace::analyze_spectrum(samples, 64);
    const auto raw = humtrace::analyze_spectrum(samples, 64, false);
    require(std::isinf(ac[0].level_dbfs), "mean removal eliminates a constant signal");
    require(std::abs(raw[0].level_dbfs + 12.0411998266) < 1e-8,
            "disabled mean removal retains the coherent-gain-corrected DC amplitude");
    require(samples.front() == 0.25 && samples.back() == 0.25, "detrending preserves source samples");
}
} // namespace

int main() {
    absent_floor_does_not_establish_peak_or_harmonic_prominence();
    a_flat_maximum_is_not_interpolated();
    peak_metadata_distinguishes_resolution_from_uncertainty();
    configurable_peak_search_honors_band_separation_and_floor();
    configured_harmonic_search_measures_off_nominal_local_maxima();
    spectrum_detrending_is_explicit_and_preserves_the_input();
    unresolvable_harmonics_yield_no_evidence_and_huge_samples_are_rejected();
    dc_offset_does_not_masquerade_as_the_strongest_component();
    finds_multiple_prominent_peaks();
    finds_peak_frequency_and_level_for_a_sine();
    rejects_non_power_of_two_input();
    measures_harmonic_support_without_source_attribution();
    tracks_the_strongest_component_in_non_overlapping_frames();
    reports_50_and_60_hz_harmonic_measurements_per_frame();
    represents_silence_without_inventing_a_peak_frequency();
    harmonic_support_requires_prominence_over_local_noise();
    interpolates_frequency_between_fft_bins();
    std::cout << "All spectrum tests passed.\n";
}
