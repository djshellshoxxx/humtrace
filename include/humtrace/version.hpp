#pragma once

namespace humtrace {
inline constexpr const char* kApplicationVersion = "0.2.4";
// Bump when estimator behavior changes (window, DC removal, interpolation, peak search, tracking).
inline constexpr const char* kMethodVersion = "hann-fft-configurable/2+welch-hann-configurable/2+raw-levels/1+ambiguity-tracker/2";
inline constexpr const char* kReportSchemaVersion = "1.3";
} // namespace humtrace
