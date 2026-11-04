#include "humtrace/report.hpp"

#include "humtrace/version.hpp"
#include "json_writer.hpp"

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
void write_track(std::ostream& output, const ToneTrack& t, std::uint32_t sample_rate_hz) {
    const double rate = sample_rate_hz == 0 ? 0.0 : static_cast<double>(sample_rate_hz);
    output << "{\"start_sample\": " << t.start_sample << ", \"end_sample\": " << t.end_sample
           << ", \"start_seconds\": ";
    write_number(output, rate > 0.0 ? static_cast<double>(t.start_sample) / rate : std::nan(""));
    output << ", \"end_seconds\": ";
    write_number(output, rate > 0.0 ? static_cast<double>(t.end_sample) / rate : std::nan(""));
    output << ", \"frames_detected\": " << t.frames_detected
           << ", \"frames_spanned\": " << t.frames_spanned << ", \"persistence\": ";
    write_number(output, t.persistence);
    output << ", \"median_frequency_hz\": ";
    write_number(output, t.median_frequency_hz);
    output << ", \"min_frequency_hz\": ";
    write_number(output, t.min_frequency_hz);
    output << ", \"max_frequency_hz\": ";
    write_number(output, t.max_frequency_hz);
    output << ", \"drift_hz_per_second\": ";
    write_number(output, t.drift_hz_per_second);
    output << ", \"median_level_dbfs\": ";
    write_number(output, t.median_level_dbfs);
    output << ", \"max_level_dbfs\": ";
    write_number(output, t.max_level_dbfs);
    output << ", \"median_prominence_db\": ";
    write_number(output, t.median_prominence_db);
    output << ", \"near_mains_multiple\": ";
    if (t.mains.nominal_hz <= 0.0) {
        output << "null";
    } else {
        output << "{\"nominal_hz\": ";
        write_number(output, t.mains.nominal_hz);
        output << ", \"harmonic\": " << t.mains.harmonic << ", \"deviation_hz\": ";
        write_number(output, t.mains.deviation_hz);
        output << '}';
    }
    output << '}';
}
} // namespace

std::string serialize_report_json(const AnalysisReport& report) {
    std::ostringstream output;
    output << std::setprecision(17)
           << "{\n  \"schema_version\": \"" << kSchemaVersion << "\",\n  \"application\": \"HumTrace\",\n  \"application_version\": ";
    write_string(output, report.application_version.empty() ? std::string(kVersion) : report.application_version);
    output << ",\n  \"input\": {\n    \"path\": ";
    write_string(output, report.input_path);
    output << ",\n    \"sha256\": ";
    write_string(output, report.input_sha256);
    output << ",\n    \"size_bytes\": " << report.input_size_bytes;
    output << ",\n    \"sample_rate_hz\": " << report.sample_rate_hz
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
    output << ",\n    \"tone_tracking\": {\"max_peaks_per_frame\": " << report.max_tonal_peaks
           << ", \"frequency_tolerance_hz\": ";
    write_number(output, report.track_tolerance_hz);
    output << ", \"max_gap_frames\": " << report.track_max_gap_frames
           << ", \"min_frames\": " << report.track_min_frames
           << ", \"truncated\": " << (report.tracks_truncated ? "true" : "false") << '}';
    output << "\n  },\n  \"channels\": [";
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
        output << "], \"tracks\": [";
        for (std::size_t t = 0; t < channel.tracks.size(); ++t) {
            output << (t == 0 ? "\n      " : ",\n      ");
            write_track(output, channel.tracks[t], report.sample_rate_hz);
        }
        if (!channel.tracks.empty()) output << '\n' << "    ";
        output << "]}";
    }
    if (!report.channel_results.empty()) output << '\n' << "  ";
    output << "],\n  \"warnings\": [";
    for (std::size_t i = 0; i < report.warnings.size(); ++i) {
        if (i != 0) output << ", ";
        write_string(output, report.warnings[i]);
    }
    output << "],\n  \"interpretation\": {\n    \"source_classification\": null,\n"
           << "    \"note\": \"Frequency, harmonic and tone-track measurements do not prove a physical source. near_mains_multiple is frequency proximity only.\"\n  }\n}\n";
    return output.str();
}

} // namespace humtrace
