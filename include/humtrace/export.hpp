#pragma once

#include "humtrace/report.hpp"

#include <string>

namespace humtrace {

// One row per reported track:
// channel,track_id,start_seconds,end_seconds,median_hz,note,cents,median_dbfs,drift_hz_per_s,observed_frames,missing_frames
// Descriptive measurements only; note names are equal-tempered labels, not source claims.
[[nodiscard]] std::string serialize_tracks_csv(const AnalysisReport& report, double a4_hz = 440.0);

} // namespace humtrace
