#pragma once

#include <cstddef>
#include <span>

namespace humtrace {
inline constexpr double sample_level_floor_dbfs = -300.0;

// Unwindowed, raw decoded samples, including DC. RMS reference is amplitude 1.0;
// a full-scale sine is -3.0103 dBFS RMS. Sample peak is not an intersample true peak.
struct SampleLevels {
    std::size_t sample_count{};
    double mean{};
    double rms_dbfs{sample_level_floor_dbfs};
    double sample_peak_dbfs{sample_level_floor_dbfs};
    bool rms_below_numeric_floor{true};
    bool peak_below_numeric_floor{true};
};

// Rejects empty or non-finite input. Scaled squares avoid overflow/underflow.
[[nodiscard]] SampleLevels measure_sample_levels(std::span<const double> samples);
} // namespace humtrace
