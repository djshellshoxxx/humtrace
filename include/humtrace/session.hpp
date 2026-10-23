#pragma once

#include "humtrace/pipeline.hpp"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace humtrace {

enum class Stage { Hashing, Analyzing, Finalizing, Done };

struct SessionCallbacks {
    std::function<void(Stage, double)> progress; // fraction in [0, 1] within the stage
    std::function<bool()> cancelled;
};

class DiskAnalysis;

struct AnalysisResult {
    AnalysisReport report;
    std::shared_ptr<DiskAnalysis> storage; // optional immutable disk-backed timeline
    std::vector<ChannelView> views; // filled only when want_views is true
};

// All path arguments are UTF-8 (converted to the native encoding, so non-ASCII names work on Windows).
[[nodiscard]] std::filesystem::path path_from_utf8(const std::string& utf8);

// Read-only analysis: hash bytes, stream-analyze, then rehash for consistency.
// Same metadata and hashes do not establish an immutable snapshot or recording origin.
// Throws std::runtime_error (including AnalysisCancelled) on failure; never writes the input.
[[nodiscard]] AnalysisResult analyze_file(const std::string& path, const PipelineSettings& settings,
                                          const SessionCallbacks& callbacks, bool want_views, FrameSink* sink = nullptr);

// Throws std::runtime_error if `output` and `input` name the same file (exports must never
// overwrite a recording). Safe when either path does not exist yet.
void require_distinct_paths(const std::string& output, const std::string& input);

// Write text to `path` by writing a sibling temp file then renaming, so a failed export
// never leaves a truncated report. With replace_existing=false, publication also
// refuses a destination created concurrently after preflight. Throws on failure.
void write_file_atomically(const std::string& path, const std::function<void(std::ostream&)>& writer,
                           bool replace_existing = true);
void write_file_atomically(const std::string& path, const std::string& content,
                           bool replace_existing = true);

} // namespace humtrace
