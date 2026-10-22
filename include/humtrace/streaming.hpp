#pragma once

#include "humtrace/session.hpp"

namespace humtrace {

struct StreamingLimits {
    std::uint64_t max_disk_bytes{4ULL * 1024 * 1024 * 1024};
    std::uint64_t min_free_disk_bytes{16ULL * 1024 * 1024};
    std::uint64_t max_resident_bytes{512ULL * 1024 * 1024};
    std::size_t max_active_tracks{4096};
};

struct StreamingResult {
    AnalysisReport summary; // no retained frames or track observations
    std::vector<std::uint64_t> frame_counts;
};

// Incrementally spool frames and observations on the output filesystem, then
// publish a schema-compatible JSON report atomically. Exact medians use bounded
// disk scans rather than retaining an observation vector. All scratch is removed
// on success, cancellation, quota exhaustion or failure. Input is never modified.
[[nodiscard]] StreamingResult stream_file_report(
    const std::string& input, const std::string& output, const PipelineSettings& settings,
    const SessionCallbacks& callbacks = {}, const StreamingLimits& limits = {},
    bool replace_existing = false, bool full_input_path = false);

} // namespace humtrace
