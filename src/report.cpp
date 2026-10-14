#include "humtrace/report.hpp"

#include "humtrace/json.hpp"
#include "humtrace/version.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace humtrace {
namespace {
using json::write_number;
using json::write_string;

void write_candidate(std::ostream& output, const HarmonicCandidate& candidate) {
    output << "{\"nominal_frequency_hz\": ";
    write_number(output, candidate.nominal_frequency_hz);
    output << ", \"supporting_harmonics\": " << candidate.supporting_harmonics
           << ", \"harmonics\": [";
    for (std::size_t i = 0; i < candidate.harmonics.size(); ++i) {
        const auto& harmonic = candidate.harmonics[i];
        if (i != 0) output << ", ";
        output << "{\"number\": " << harmonic.harmonic_number << ", \"expected_frequency_hz\": ";
        write_number(output, harmonic.expected_frequency_hz);
        output << ", \"measured_bin_hz\": ";
        write_number(output, harmonic.measured_bin_hz);
        output << ", \"level_dbfs\": ";
        write_number(output, harmonic.level_dbfs);
        output << ", \"local_floor_dbfs\": ";
        write_number(output, harmonic.local_floor_dbfs);
        output << ", \"prominence_db\": ";
        write_number(output, harmonic.prominence_db);
        output << ", \"supported\": " << (harmonic.supports_threshold ? "true" : "false") << '}';
    }
    output << "]}";
}
} // namespace

std::string serialize_report_json(const AnalysisReport& report) {
    std::ostringstream output;
    output << std::setprecision(17)
           << "{\n  \"schema_version\": \"" << kReportSchemaVersion << "\",\n  \"application\": \"HumTrace\",\n  \"generator\": {\"application_version\": \"" << kApplicationVersion
           << "\", \"method_version\": \"" << kMethodVersion << "\"},\n  \"input\": {\n    \"path\": ";
    write_string(output, report.input_path);
    output << ",\n    \"sha256\": ";
    if (report.input_sha256.empty())
        output << "null";
    else
        write_string(output, report.input_sha256);
    output << ",\n    \"bytes\": " << report.input_bytes
           << ",\n    \"sample_rate_hz\": " << report.sample_rate_hz
           << ",\n    \"channels\": " << report.channels
           << ",\n    \"bit_depth\": " << report.bit_depth
           << ",\n    \"format_code\": " << report.format_code
           << ",\n    \"duration_seconds\": ";
    write_number(output, report.duration_seconds);
    output << "\n  },\n  \"settings\": {\n    \"window\": \"hann\",\n    \"frame_size_samples\": "
           << report.frame_size_samples << ",\n    \"hop_size_samples\": " << report.hop_size_samples
           << ",\n    \"support_threshold_dbfs\": ";
    write_number(output, report.support_threshold_dbfs);
    output << ",\n    \"minimum_prominence_db\": ";
    write_number(output, report.minimum_prominence_db);
    output << ",\n    \"tracker\": {\"fixed_hz_tolerance\": ";
    write_number(output, report.tracker.fixed_hz_tolerance);
    output << ", \"relative_tolerance\": ";
    write_number(output, report.tracker.relative_tolerance);
    output << ", \"bin_spacing_hz\": ";
    write_number(output, report.tracker.bin_spacing_hz);
    output << ", \"bin_fraction\": ";
    write_number(output, report.tracker.bin_fraction);
    output << ", \"max_gap_frames\": " << report.tracker.max_gap_frames
           << ", \"min_observed_frames\": " << report.tracker.min_observed_frames;
    output << "},\n    \"pipeline\": {\"peak_min_level_dbfs\": ";
    write_number(output, report.peak_min_level_dbfs);
    output << ", \"peak_min_prominence_db\": ";
    write_number(output, report.peak_min_prominence_db);
    output << ", \"max_peaks_per_frame\": " << report.max_peaks_per_frame
           << ", \"max_harmonics\": " << report.max_harmonics
           << ", \"dc_removed\": true, \"hop_fraction\": 0.5, \"tail_window\": \"end-aligned\"}\n  },\n  \"warnings\": [";
    for (std::size_t i = 0; i < report.warnings.size(); ++i) {
        if (i != 0) output << ", ";
        write_string(output, report.warnings[i]);
    }
    output << "],\n  \"channels\": [";
    for (std::size_t channel_index = 0; channel_index < report.channel_results.size(); ++channel_index) {
        const auto& channel = report.channel_results[channel_index];
        if (channel_index != 0) output << ',';
        output << "\n    {\"channel_number\": " << channel.channel_number << ", \"frames\": [";
        for (std::size_t frame_index = 0; frame_index < channel.frames.size(); ++frame_index) {
            const auto& frame = channel.frames[frame_index];
            if (frame_index != 0) output << ',';
            output << "\n      {\"start_sample\": " << frame.start_sample << ", \"start_seconds\": ";
            if (report.sample_rate_hz == 0)
                output << "null";
            else
                write_number(output, static_cast<double>(frame.start_sample) / report.sample_rate_hz);
            output << ", \"strongest_peak\": {\"frequency_hz\": ";
            write_number(output, frame.strongest_peak.frequency_hz);
            output << ", \"level_dbfs\": ";
            write_number(output, frame.strongest_peak.level_dbfs);
            output << "}, \"mains_50_hz_measurements\": ";
            write_candidate(output, frame.mains_50);
            output << ", \"mains_60_hz_measurements\": ";
            write_candidate(output, frame.mains_60);
            output << '}';
        }
        if (!channel.frames.empty()) output << '\n' << "    ";
        output << "], \"suppressed_tracks\": " << channel.suppressed_tracks << ", \"clipped_samples\": " << channel.clipped_samples
               << ", \"tracks\": [";
        for (std::size_t t = 0; t < channel.tracks.size(); ++t) {
            const auto& track = channel.tracks[t];
            if (t != 0) output << ", ";
            output << "{\"local_id\": " << track.id << ", \"first_frame\": " << track.first_frame
                   << ", \"last_frame\": " << track.last_frame
                   << ", \"observed_frames\": " << track.observed_frames
                   << ", \"missing_frames\": " << track.missing_frames
                   << ", \"median_frequency_hz\": ";
            write_number(output, track.median_frequency_hz);
            output << ", \"median_level_dbfs\": ";
            write_number(output, track.median_level_dbfs);
            output << ", \"drift_hz_per_second\": ";
            write_number(output, track.drift_hz_per_second);
            output << ", \"observations\": [";
            for (std::size_t o = 0; o < track.observations.size(); ++o) {
                const auto& obs = track.observations[o];
                if (o != 0) output << ", ";
                output << "{\"frame_index\": " << obs.frame_index << ", \"start_sample\": " << obs.start_sample
                       << ", \"frequency_hz\": ";
                write_number(output, obs.frequency_hz);
                output << ", \"level_dbfs\": ";
                write_number(output, obs.level_dbfs);
                output << '}';
            }
            output << "]}";
        }
        output << "]}";
    }
    if (!report.channel_results.empty()) output << '\n' << "  ";
    output << "],\n  \"interpretation\": {\n    \"source_classification\": null,\n"
           << "    \"note\": \"Frequency and harmonic measurements do not prove a physical source.\"\n  }\n}\n";
    return output.str();
}

} // namespace humtrace
