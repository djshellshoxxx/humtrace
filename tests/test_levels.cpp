#include "humtrace/levels.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    std::vector<double> sine(1024);
    for (std::size_t i = 0; i < sine.size(); ++i)
        sine[i] = std::sin(2 * std::numbers::pi * static_cast<double>(i) / 64);
    const auto tone = humtrace::measure_sample_levels(sine);
    require(std::abs(tone.rms_dbfs + 3.01029995664) < 1e-9, "full-scale sine RMS reference");
    require(std::abs(tone.sample_peak_dbfs) < 1e-12 && std::abs(tone.mean) < 1e-12, "sample peak and signed DC");
    require(tone.sample_count == 1024 && !tone.rms_below_numeric_floor, "sample count and validity");
    const auto dc = humtrace::measure_sample_levels(std::vector<double>(16, -0.5));
    require(dc.mean == -0.5 && std::abs(dc.rms_dbfs + 6.02059991328) < 1e-9, "RMS includes raw DC");
    const auto silence = humtrace::measure_sample_levels(std::vector<double>(16, 0));
    require(silence.rms_below_numeric_floor && silence.peak_below_numeric_floor &&
            std::isfinite(silence.rms_dbfs), "silence has a finite floor and explicit flags");
    const auto tiny = humtrace::measure_sample_levels(std::vector<double>(16, 1e-300));
    require(tiny.rms_below_numeric_floor && tiny.rms_dbfs == humtrace::sample_level_floor_dbfs, "tiny values do not underflow RMS");
    const auto huge = humtrace::measure_sample_levels(std::vector<double>(16, 1e200));
    require(std::isfinite(huge.rms_dbfs) && std::abs(huge.rms_dbfs - 4000) < 1e-9, "scaled squares avoid overflow");
    for (const auto& samples : {std::vector<double>{}, std::vector<double>{NAN}, std::vector<double>{INFINITY}}) {
        bool rejected = false;
        try { (void)humtrace::measure_sample_levels(samples); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "empty and nonfinite inputs rejected");
    }
    std::cout << "All sample level tests passed.\n";
}
