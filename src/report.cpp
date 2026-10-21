#include "humtrace/report.hpp"

#include <cmath>
#include <iomanip>
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

void write_seconds(std::ostream& output, std::uint64_t sample, std::uint32_t sample_rate_hz) {
    if (sample_rate_hz == 0)
        output << "null";
    else
        write_number(output, static_cast<double>(sample) / sample_rate_hz);
}

void write_track(std::ostream& output, const PeakTrack& track, std::uint32_t sample_rate_hz) {
    output << "{\"track_number\": " << track.track_number
           << ", \"start_sample\": " << track.start_sample << ", \"start_seconds\": ";
    write_seconds(output, track.start_sample, sample_rate_hz);
    output << ", \"end_sample\": " << track.end_sample << ", \"end_seconds\": ";
    write_seconds(output, track.end_sample, sample_rate_hz);
    output << ", \"observed_frames\": " << track.observations.size()
           << ", \"missing_frames\": " << track.missing_frames
           << ", \"mean_frequency_hz\": ";
    write_number(output, track.mean_frequency_hz);
    output << ", \"min_frequency_hz\": ";
    write_number(output, track.min_frequency_hz);
    output << ", \"max_frequency_hz\": ";
    write_number(output, track.max_frequency_hz);
    output << ", \"mean_level_dbfs\": ";
    write_number(output, track.mean_level_dbfs);
    output << ", \"max_level_dbfs\": ";
    write_number(output, track.max_level_dbfs);
    output << ", \"frequency_drift_hz\": ";
    write_number(output, track.frequency_drift_hz);
    output << ", \"drift_rate_hz_per_second\": ";
    write_number(output, track.drift_rate_hz_per_second);
    output << ", \"source_classification\": null, \"observations\": [";
    for (std::size_t i = 0; i < track.observations.size(); ++i) {
        const auto& observation = track.observations[i];
        if (i != 0) output << ", ";
        output << "{\"start_sample\": " << observation.start_sample << ", \"start_seconds\": ";
        write_seconds(output, observation.start_sample, sample_rate_hz);
        output << ", \"frequency_hz\": ";
        write_number(output, observation.frequency_hz);
        output << ", \"level_dbfs\": ";
        write_number(output, observation.level_dbfs);
        output << ", \"prominence_db\": ";
        write_number(output, observation.prominence_db);
        output << '}';
    }
    output << "]}";
}
void write_psd_harmonics(std::ostream& output, const PsdHarmonicCandidate& candidate) {
    output << "{\"nominal_frequency_hz\": ";
    write_number(output, candidate.nominal_frequency_hz);
    output << ", \"prominent_harmonics\": " << candidate.prominent_harmonics
           << ", \"harmonics\": [";
    for (std::size_t i = 0; i < candidate.harmonics.size(); ++i) {
        const auto& harmonic = candidate.harmonics[i];
        if (i != 0) output << ", ";
        output << "{\"number\": " << harmonic.harmonic_number << ", \"expected_frequency_hz\": ";
        write_number(output, harmonic.expected_frequency_hz);
        output << ", \"measured_bin_hz\": ";
        write_number(output, harmonic.measured_bin_hz);
        output << ", \"level_dbfs2_per_hz\": ";
        write_number(output, harmonic.level_dbfs2_per_hz);
        output << ", \"local_floor_dbfs2_per_hz\": ";
        write_number(output, harmonic.local_floor_dbfs2_per_hz);
        output << ", \"prominence_db\": ";
        write_number(output, harmonic.prominence_db);
        output << ", \"prominent\": " << (harmonic.prominent ? "true" : "false") << '}';
    }
    output << "]}";
}

void write_psd_summary(std::ostream& output, const PsdSummary& summary) {
    output << "{\"segments_averaged\": " << summary.segments_averaged
           << ", \"samples_processed\": " << summary.samples_processed
           << ", \"bin_width_hz\": ";
    write_number(output, summary.bin_width_hz);
    output << ", \"density_unit\": \"dBFS^2/Hz\", \"broadband_median_dbfs2_per_hz\": ";
    write_number(output, summary.broadband_median_dbfs2_per_hz);
    output << ", \"source_classification\": null, \"mains_50_hz_psd\": ";
    write_psd_harmonics(output, summary.mains_50);
    output << ", \"mains_60_hz_psd\": ";
    write_psd_harmonics(output, summary.mains_60);
    output << '}';
}
} // namespace

std::string serialize_report_json(const AnalysisReport& report) {
    std::ostringstream output;
    output << std::setprecision(17)
           << "{\n  \"schema_version\": \"1.2\",\n  \"application\": \"HumTrace\",\n  \"input\": {\n    \"path\": ";
    write_string(output, report.input_path);
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
    output << ",\n    \"peak_tracking\": {\"peaks_per_frame\": " << report.peak_picking.max_peaks
           << ", \"dynamic_range_db\": ";
    write_number(output, report.peak_picking.dynamic_range_db);
    output << ", \"exclusion_bins\": " << report.peak_picking.exclusion_bins
           << ", \"frequency_tolerance_hz\": ";
    write_number(output, report.track_settings.frequency_tolerance_hz);
    output << ", \"max_gap_frames\": " << report.track_settings.max_gap_frames
           << ", \"min_observed_frames\": " << report.track_settings.min_observed_frames
           << "}";
    if (report.welch_psd) {
        const auto& welch = *report.welch_psd;
        output << ",\n    \"welch_psd\": {\"window\": \"hann\", \"density_unit\": \"dBFS^2/Hz\""
               << ", \"segment_size_samples\": " << welch.segment_size
               << ", \"hop_size_samples\": " << welch.hop_size
               << ", \"guard_bins\": " << report.psd_noise_floor.guard_bins
               << ", \"radius_bins\": " << report.psd_noise_floor.radius_bins
               << ", \"max_harmonics\": " << report.psd_noise_floor.max_harmonics
               << ", \"minimum_prominence_db\": ";
        write_number(output, report.psd_noise_floor.minimum_prominence_db);
        output << "}";
    } else {
        output << ",\n    \"welch_psd\": null";
    }
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
        for (std::size_t track_index = 0; track_index < channel.tracks.size(); ++track_index) {
            if (track_index != 0) output << ", ";
            write_track(output, channel.tracks[track_index], report.sample_rate_hz);
        }
        output << "], \"psd\": ";
        if (channel.psd)
            write_psd_summary(output, *channel.psd);
        else
            output << "null";
        output << '}';
    }
    if (!report.channel_results.empty()) output << '\n' << "  ";
    output << "],\n  \"interpretation\": {\n    \"source_classification\": null,\n"
           << "    \"note\": \"Frequency, harmonic, PSD, and track measurements do not prove a physical source or cause. Tracks summarize repeated spectral peaks only; PSD prominence flags summarize averaged spectral density only.\"\n  }\n}\n";
    return output.str();
}

} // namespace humtrace
