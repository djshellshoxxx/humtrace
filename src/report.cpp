#include "humtrace/report.hpp"

#include "humtrace/music.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace humtrace {
namespace {
void write_string(std::ostream& output, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char byte : value) {
        switch (byte) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (byte < 0x20)
                output << "\\u00" << hex[byte >> 4] << hex[byte & 0x0f];
            else
                output << static_cast<char>(byte);
        }
    }
    output << '"';
}

void write_number(std::ostream& output, double value) {
    if (std::isfinite(value))
        output << value;
    else
        output << "null";
}

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

void write_note(std::ostream& output, double frequency_hz, double a4_hz) {
    if (!std::isfinite(frequency_hz) || frequency_hz <= 0.0) {
        output << "null";
        return;
    }
    const auto note = describe_note(frequency_hz, a4_hz);
    output << "{\"name\": ";
    write_string(output, note.name);
    output << ", \"midi\": " << note.midi_note << ", \"cents\": ";
    write_number(output, note.cents);
    output << '}';
}

void write_track(std::ostream& output, const Track& track, double sample_rate_hz, double a4_hz) {
    const auto seconds = [&](std::uint64_t sample) {
        return sample_rate_hz > 0.0 ? static_cast<double>(sample) / sample_rate_hz
                                    : std::numeric_limits<double>::quiet_NaN();
    };
    output << "{\"id\": " << track.id << ", \"first_sample\": " << track.first_sample
           << ", \"last_sample\": " << track.last_sample << ", \"first_seconds\": ";
    write_number(output, seconds(track.first_sample));
    output << ", \"last_seconds\": ";
    write_number(output, seconds(track.last_sample));
    output << ", \"observed_frames\": " << track.observations.size()
           << ", \"missing_frames\": " << track.missing_frames << ", \"median_frequency_hz\": ";
    write_number(output, track.median_frequency_hz);
    output << ", \"median_level_dbfs\": ";
    write_number(output, track.median_level_dbfs);
    output << ", \"drift_hz_per_second\": ";
    write_number(output, track.drift_hz_per_second);
    output << ", \"nearest_note\": ";
    write_note(output, track.median_frequency_hz, a4_hz);
    output << ", \"end_reason\": ";
    write_string(output, track.end_reason);
    output << ", \"observations\": [";
    for (std::size_t i = 0; i < track.observations.size(); ++i) {
        const auto& o = track.observations[i];
        if (i != 0) output << ", ";
        output << "{\"start_sample\": " << o.start_sample << ", \"frequency_hz\": ";
        write_number(output, o.frequency_hz);
        output << ", \"level_dbfs\": ";
        write_number(output, o.level_dbfs);
        output << ", \"prominence_db\": ";
        write_number(output, o.prominence_db);
        output << '}';
    }
    output << "]}";
}

void write_id_list(std::ostream& output, const std::vector<std::size_t>& ids) {
    output << '[';
    for (std::size_t i = 0; i < ids.size(); ++i)
        output << (i != 0 ? ", " : "") << ids[i];
    output << ']';
}

void write_comparison(std::ostream& output, const ReportComparison& comparison) {
    output << ",\n  \"comparison\": {\n    \"mode\": \"independent_summary\",\n"
           << "    \"channel_mapping\": \"same_index\",\n    \"alignment\": \"none\",\n"
           << "    \"resampling\": \"none\",\n    \"reference\": {\"path\": ";
    write_string(output, comparison.reference_name);
    output << ", \"sha256\": ";
    write_string(output, comparison.reference_sha256);
    output << ", \"size_bytes\": " << comparison.reference_size_bytes
           << ", \"sample_rate_hz\": " << comparison.reference_sample_rate_hz
           << "},\n    \"channels\": [";
    for (std::size_t c = 0; c < comparison.channels.size(); ++c) {
        const auto& channel = comparison.channels[c];
        output << (c != 0 ? "," : "") << "\n      {\"channel_number\": " << channel.channel_number
               << ", \"frequency_tolerance_hz\": ";
        write_number(output, channel.comparison.frequency_tolerance_hz);
        output << ", \"matches\": [";
        for (std::size_t m = 0; m < channel.comparison.matches.size(); ++m) {
            const auto& match = channel.comparison.matches[m];
            output << (m != 0 ? ", " : "") << "{\"track_id\": " << match.track_a_id
                   << ", \"reference_track_id\": " << match.track_b_id << ", \"delta_frequency_hz\": ";
            write_number(output, match.delta_frequency_hz);
            output << ", \"delta_level_db\": ";
            write_number(output, match.delta_level_db);
            output << ", \"delta_cents\": ";
            write_number(output, match.delta_cents);
            output << '}';
        }
        output << "], \"unmatched_track_ids\": ";
        write_id_list(output, channel.comparison.unmatched_a);
        output << ", \"unmatched_reference_track_ids\": ";
        write_id_list(output, channel.comparison.unmatched_b);
        output << '}';
    }
    output << "\n    ]\n  }";
}
} // namespace

std::string serialize_report_json(const AnalysisReport& report) {
    std::ostringstream output;
    output << std::setprecision(17)
           << "{\n  \"schema_version\": \"1.1\",\n  \"application\": \"HumTrace\",\n  \"input\": {\n    \"path\": ";
    write_string(output, report.input_path);
    output << ",\n    \"size_bytes\": " << report.input_size_bytes
           << ",\n    \"hash_algorithm\": \"SHA-256\",\n    \"sha256\": ";
    write_string(output, report.input_sha256);
    output << ",\n    \"unchanged_during_analysis\": "
           << (report.input_unchanged_during_analysis ? "true" : "false")
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
    output << ",\n    \"a4_reference_hz\": ";
    write_number(output, report.a4_reference_hz);
    output << ",\n    \"peak_detection\": {\"max_peaks\": " << report.peak_settings.max_peaks
           << ", \"minimum_level_dbfs\": ";
    write_number(output, report.peak_settings.minimum_level_dbfs);
    output << ", \"minimum_prominence_db\": ";
    write_number(output, report.peak_settings.minimum_prominence_db);
    output << "},\n    \"tracker\": {\"fixed_hz_tolerance\": ";
    write_number(output, report.tracker_settings.fixed_hz_tolerance);
    output << ", \"relative_tolerance\": ";
    write_number(output, report.tracker_settings.relative_tolerance);
    output << ", \"bin_fraction_tolerance\": ";
    write_number(output, report.tracker_settings.bin_fraction_tolerance);
    output << ", \"max_gap_frames\": " << report.tracker_settings.max_gap_frames
           << ", \"min_observations\": " << report.tracker_settings.min_observations
           << "}\n  },\n  \"channels\": [";
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
            output << ", \"nearest_note\": ";
            write_note(output, frame.strongest_peak.frequency_hz, report.a4_reference_hz);
            output << "}, \"mains_50_hz_measurements\": ";
            write_candidate(output, frame.mains_50);
            output << ", \"mains_60_hz_measurements\": ";
            write_candidate(output, frame.mains_60);
            output << '}';
        }
        if (!channel.frames.empty()) output << '\n' << "    ";
        output << "], \"tracks\": [";
        for (std::size_t track_index = 0; track_index < channel.tracks.size(); ++track_index) {
            output << (track_index != 0 ? "," : "") << "\n      ";
            write_track(output, channel.tracks[track_index], report.sample_rate_hz, report.a4_reference_hz);
        }
        if (!channel.tracks.empty()) output << '\n' << "    ";
        output << "]}";
    }
    if (!report.channel_results.empty()) output << '\n' << "  ";
    output << "],\n  \"interpretation\": {\n    \"source_classification\": null,\n"
           << "    \"note\": \"Frequency and harmonic measurements do not prove a physical source.\"\n  }";
    if (report.comparison)
        write_comparison(output, *report.comparison);
    output << "\n}\n";
    return output.str();
}

} // namespace humtrace
