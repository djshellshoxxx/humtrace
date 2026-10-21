#include "humtrace/export.hpp"

#include "humtrace/music.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace humtrace {

std::string serialize_tracks_csv(const AnalysisReport& report, double a4_hz) {
    std::ostringstream out;
    out << std::setprecision(10)
        << "channel,track_id,start_seconds,end_seconds,median_hz,note,cents,median_dbfs,"
           "drift_hz_per_s,observed_frames,missing_frames\n";
    const auto rate = static_cast<double>(report.sample_rate_hz);
    for (const auto& channel : report.channel_results) {
        for (const auto& track : channel.tracks) {
            if (track.observations.empty()) continue;
            const auto start = static_cast<double>(track.observations.front().start_sample) / rate;
            const auto end = (static_cast<double>(track.observations.back().start_sample) +
                              static_cast<double>(report.frame_size_samples)) / rate;
            out << channel.channel_number << ',' << track.id << ',' << start << ',' << end << ','
                << track.median_frequency_hz << ',';
            if (track.median_frequency_hz > 0.0) {
                const auto note = nearest_note(track.median_frequency_hz, a4_hz);
                out << note.name << ',' << note.cents;
            } else {
                out << ',';
            }
            out << ',';
            if (std::isfinite(track.median_level_dbfs)) out << track.median_level_dbfs;
            out << ',' << track.drift_hz_per_second << ',' << track.observed_frames << ','
                << track.missing_frames << '\n';
        }
    }
    return out.str();
}

} // namespace humtrace
