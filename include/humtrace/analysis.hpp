// Purpose: UI-independent file analysis engine (streaming WAV -> AnalysisReport).
// Used by the CLI and the Qt GUI. Never writes to or modifies the input file.
#pragma once

#include "humtrace/report.hpp"
#include "humtrace/spectrum.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace humtrace {

struct AnalysisOptions {
    // 0 = choose automatically (largest power of two <= min(sample rate, 65536)).
    std::size_t frame_size_samples{0};
    double support_threshold_dbfs{-60.0};
    double minimum_prominence_db{10.0};
    // Hard cap on retained (frame x channel) results, bounding report memory.
    std::size_t max_result_frames{200000};
    // Optional; polled between frames. Setting it true stops analysis.
    const std::atomic<bool>* cancel{nullptr};
    // Optional; called with a fraction in [0,1] roughly once per window.
    std::function<void(double)> progress;
};

class AnalysisCancelled : public std::runtime_error {
public:
    AnalysisCancelled() : std::runtime_error("analysis cancelled") {}
};

// Throws std::runtime_error (clear message) on invalid input or limits,
// AnalysisCancelled on cancellation. Hashes the input bytes for the report.
[[nodiscard]] AnalysisReport analyze_wav_file(const std::string& path,
                                              const AnalysisOptions& options = {});

// Spectrum of one frame (for display). `start_sample` must be a frame start from
// a prior analysis. Throws std::runtime_error on invalid arguments.
[[nodiscard]] std::vector<SpectrumBin> load_frame_spectrum(const std::string& path,
                                                           std::uint16_t channel_number,
                                                           std::uint64_t start_sample,
                                                           std::size_t frame_size);

// Writes `contents` to `path` atomically (temp file + rename). An existing
// file is first copied to `path + ".bak"`. Refuses to touch `input_path`.
void write_file_safely(const std::string& path, const std::string& contents,
                       const std::string& input_path);

} // namespace humtrace
