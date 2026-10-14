#pragma once

namespace humtrace {
inline constexpr const char* kApplicationVersion = "0.1.0";
// Bump when estimator behavior changes (window, DC removal, interpolation, peak search, tracking).
inline constexpr const char* kMethodVersion = "hann-fft-parabolic-dc-removed/1";
inline constexpr const char* kReportSchemaVersion = "1.2";
} // namespace humtrace
