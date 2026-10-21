#pragma once

#include "humtrace/report.hpp"

#include <atomic>
#include <functional>
#include <stdexcept>
#include <string>

namespace humtrace {

struct AnalysisOptions {
    PeakSettings peaks;
    TrackerSettings tracker;
    double a4_reference_hz{440.0};
    bool record_full_path{false};  // default: basename only, for privacy
    const std::atomic<bool>* cancel{};
    std::function<void(double)> progress;  // fraction in [0, 1]
};

class AnalysisCancelled : public std::runtime_error {
public:
    AnalysisCancelled() : std::runtime_error("analysis cancelled") {}
};

// Hash, stream-decode, and analyze a WAV file without modifying it. Throws
// AnalysisCancelled when options.cancel becomes true, std::runtime_error on
// invalid input. Partial results are never returned.
[[nodiscard]] AnalysisReport analyze_wav_file(const std::string& path, const AnalysisOptions& options);

// Write text to path atomically (temp file in the same directory, then rename).
// Throws std::runtime_error; leaves no partial file at path on failure.
void write_file_atomically(const std::string& path, const std::string& contents);

} // namespace humtrace
