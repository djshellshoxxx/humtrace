#include "humtrace/compare.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace humtrace {
namespace {
double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    const auto mid = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), mid, values.end());
    return *mid;
}
} // namespace

FileSummary summarize(const AnalysisReport& report, std::size_t min_support) {
    FileSummary out;
    out.path = report.input_path;
    out.sha256 = report.input_sha256;
    for (const auto& channel : report.channel_results) {
        ChannelSummary s;
        s.channel_number = channel.channel_number;
        s.frames = channel.frames.size();
        std::vector<double> hz, db;
        for (const auto& frame : channel.frames) {
            if (frame.mains_50.supporting_harmonics >= min_support) ++s.frames_50_hz_supported;
            if (frame.mains_60.supporting_harmonics >= min_support) ++s.frames_60_hz_supported;
            if (std::isfinite(frame.strongest_peak.level_dbfs)) {
                hz.push_back(frame.strongest_peak.frequency_hz);
                db.push_back(frame.strongest_peak.level_dbfs);
            }
        }
        s.median_peak_hz = median(hz);
        s.median_peak_dbfs = median(db);
        out.channels.push_back(s);
    }
    return out;
}

std::string format_comparison(const std::vector<FileSummary>& files) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(2);
    o << "HumTrace comparison (measurements only; no source attribution)\n"
      << "Supported = frames with >=3 harmonics over threshold and prominence.\n";
    for (const auto& f : files) {
        o << "\n" << f.path << "\n  sha256 " << f.sha256 << '\n';
        for (const auto& c : f.channels)
            o << "  ch" << c.channel_number << ": frames " << c.frames << ", 50 Hz supported "
              << c.frames_50_hz_supported << ", 60 Hz supported " << c.frames_60_hz_supported
              << ", median peak " << c.median_peak_hz << " Hz / " << c.median_peak_dbfs << " dBFS\n";
    }
    return o.str();
}

} // namespace humtrace
