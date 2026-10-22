#include "humtrace/levels.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace humtrace {
SampleLevels measure_sample_levels(std::span<const double> samples) {
    if (samples.empty()) throw std::invalid_argument("sample levels need non-empty input");
    double peak = 0;
    for (const double value : samples) {
        if (!std::isfinite(value)) throw std::invalid_argument("sample levels need finite input");
        peak = std::max(peak, std::abs(value));
    }
    SampleLevels result;
    result.sample_count = samples.size();
    if (peak == 0) return result;
    double normalized_mean = 0, normalized_power = 0;
    const double count = static_cast<double>(samples.size());
    for (const double value : samples) {
        const double normalized = value / peak;
        normalized_mean += normalized / count;
        normalized_power += (normalized * normalized) / count;
    }
    result.mean = peak * std::clamp(normalized_mean, -1.0, 1.0);
    const double peak_db = 20 * std::log10(peak);
    const double rms_db = peak_db + 10 * std::log10(normalized_power);
    result.peak_below_numeric_floor = peak_db < sample_level_floor_dbfs;
    result.rms_below_numeric_floor = rms_db < sample_level_floor_dbfs;
    result.sample_peak_dbfs = std::max(peak_db, sample_level_floor_dbfs);
    result.rms_dbfs = std::max(rms_db, sample_level_floor_dbfs);
    return result;
}
} // namespace humtrace
