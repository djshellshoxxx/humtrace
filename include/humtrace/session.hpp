#pragma once

#include "humtrace/pipeline.hpp"

#include <functional>
#include <string>
#include <vector>

namespace humtrace {

enum class Stage { Hashing, Analyzing, Done };

struct SessionCallbacks {
    std::function<void(Stage, double)> progress; // fraction in [0, 1] within the stage
    std::function<bool()> cancelled;
};

struct AnalysisResult {
    AnalysisReport report;
    std::vector<ChannelView> views; // filled only when want_views is true
};

// Read-only analysis of a file on disk: hash the exact bytes, then stream-analyze.
// Throws std::runtime_error (including AnalysisCancelled) on failure; never writes the input.
[[nodiscard]] AnalysisResult analyze_file(const std::string& path, const PipelineSettings& settings,
                                          const SessionCallbacks& callbacks, bool want_views);

// Write text to `path` by writing a sibling temp file then renaming, so a failed export
// never leaves a truncated report. Throws std::runtime_error on failure.
void write_file_atomically(const std::string& path, const std::string& content);

} // namespace humtrace
