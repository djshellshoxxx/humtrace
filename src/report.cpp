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
        output << ", \"supported\": " << (harmonic.supports_threshold ? "true" : "false")
               << ", \"frequency_hz\": "; write_number(output, harmonic.frequency_hz);
        output << ", \"search_tolerance_hz\": "; write_number(output, harmonic.search_tolerance_hz);
        output << ", \"local_floor_available\": " << (harmonic.local_floor_available ? "true" : "false")
               << ", \"boundary_peak\": " << (harmonic.boundary_peak ? "true" : "false") << '}';
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
        output << ", \"prominent\": " << (harmonic.prominent ? "true" : "false")
               << ", \"frequency_hz\": "; write_number(output, harmonic.frequency_hz);
        output << ", \"search_tolerance_hz\": "; write_number(output, harmonic.search_tolerance_hz);
        output << ", \"local_floor_available\": " << (harmonic.local_floor_available ? "true" : "false") << '}';
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

void write_frame_json(std::ostream& output, const InterferenceFrame& frame, std::uint32_t sample_rate_hz) {
            const json::FormatGuard format(output);
            output << "\n      {\"start_sample\": " << frame.start_sample << ", \"start_seconds\": ";
            if (sample_rate_hz == 0)
                output << "null";
            else
                write_number(output, static_cast<double>(frame.start_sample) / sample_rate_hz);
            output << ", \"sample_levels\": ";
            if (frame.levels) {
                const auto& levels = *frame.levels;
                output << "{\"sample_count\": " << levels.sample_count << ", \"mean\": ";
                write_number(output, levels.mean);
                output << ", \"rms_dbfs\": "; write_number(output, levels.rms_dbfs);
                output << ", \"sample_peak_dbfs\": "; write_number(output, levels.sample_peak_dbfs);
                output << ", \"rms_below_numeric_floor\": " << (levels.rms_below_numeric_floor ? "true" : "false")
                       << ", \"peak_below_numeric_floor\": " << (levels.peak_below_numeric_floor ? "true" : "false")
                       << ", \"floor_dbfs\": " << sample_level_floor_dbfs
                       << ", \"reference\": \"normalized amplitude 1.0; RMS includes DC; unwindowed sample peak\"}";
            } else output << "null";
            output << ", \"strongest_peak\": {\"frequency_hz\": ";
            const bool peak_available = std::isfinite(frame.strongest_peak.level_dbfs) &&
                                        frame.strongest_peak.frequency_hz > 0.0;
            if (peak_available) write_number(output, frame.strongest_peak.frequency_hz); else output << "null";
            output << ", \"level_dbfs\": ";
            write_number(output, frame.strongest_peak.level_dbfs);
            const auto& peak = frame.strongest_peak;
            output << ", \"raw_bin_frequency_hz\": ";
            if (peak_available) write_number(output, peak.raw_bin_frequency_hz); else output << "null";
            output << ", \"bin_spacing_hz\": "; write_number(output, peak.bin_spacing_hz);
            output << ", \"hann_main_lobe_width_hz\": "; write_number(output, peak.hann_main_lobe_width_hz);
            output << ", \"local_floor_dbfs\": ";
            if (peak.local_floor_available) write_number(output, peak.local_floor_dbfs); else output << "null";
            output << ", \"prominence_db\": ";
            if (peak.local_floor_available) write_number(output, peak.prominence_db); else output << "null";
            output << ", \"local_floor_available\": " << (peak.local_floor_available ? "true" : "false")
                   << ", \"interpolation_applied\": " << (peak.interpolation_applied ? "true" : "false")
                   << ", \"boundary_peak\": " << (peak.boundary_peak ? "true" : "false")
                   << ", \"competing_peak\": " << (peak.competing_peak ? "true" : "false")
                   << ", \"width_hz\": ";
            if (peak.width_hz) write_number(output, *peak.width_hz); else output << "null";
            output << ", \"frequency_uncertainty_hz\": ";
            if (peak.frequency_uncertainty_hz) write_number(output, *peak.frequency_uncertainty_hz); else output << "null";
            output << "}, \"mains_50_hz_measurements\": ";
            write_candidate(output, frame.mains_50);
            output << ", \"mains_60_hz_measurements\": ";
            write_candidate(output, frame.mains_60);
            output << '}';
}

void write_track_json(std::ostream& output, const Track& track, const std::function<void(std::ostream&)>& observations,
                      const std::function<void(std::ostream&)>& gaps) {
            const json::FormatGuard format(output);
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
            output << ", \"frequency_q25_hz\": "; write_number(output, track.frequency_q25_hz);
            output << ", \"frequency_q75_hz\": "; write_number(output, track.frequency_q75_hz);
            output << ", \"level_q25_dbfs\": "; write_number(output, track.level_q25_dbfs);
            output << ", \"level_q75_dbfs\": "; write_number(output, track.level_q75_dbfs);
            output << ", \"quantile_method\": \"linear interpolation at (N-1)*p\", \"drift_r_squared\": ";
            if (track.drift_r_squared) write_number(output, *track.drift_r_squared); else output << "null";
            output << ", \"drift_residual_rms_hz\": ";
            if (track.drift_residual_rms_hz) write_number(output, *track.drift_residual_rms_hz); else output << "null";
            output << ", \"end_reason\": ";
            write_string(output, track.end_reason == TrackEndReason::Ambiguous ? "ambiguous" :
                                 track.end_reason == TrackEndReason::GapExceeded ? "gap_exceeded" : "end_of_input");
            output << ", \"start_ambiguity_id\": ";
            if (track.start_ambiguity_id) output << *track.start_ambiguity_id; else output << "null";
            output << ", \"end_ambiguity_id\": ";
            if (track.end_ambiguity_id) output << *track.end_ambiguity_id; else output << "null";
            output << ", \"event\": {\"analysis_local_id\": " << track.id
                   << ", \"first_frame_inclusive\": " << track.first_frame
                   << ", \"last_frame_inclusive\": " << track.last_frame
                   << ", \"observed_frames\": " << track.observed_frames << ", \"gaps\": [";
            if (gaps) gaps(output);
            else {
                bool first = true;
                for (std::size_t i = 1; i < track.observations.size(); ++i) {
                    const auto previous = track.observations[i - 1].frame_index;
                    const auto current = track.observations[i].frame_index;
                    if (current <= previous || current - previous <= 1) continue;
                    if (!first) output << ", ";
                    first = false;
                    output << "{\"first_frame_inclusive\": " << previous + 1
                           << ", \"last_frame_inclusive\": " << current - 1 << '}';
                }
            }
            output << "]}, \"observations\": [";
            if (observations) observations(output);
            else for (std::size_t o = 0; o < track.observations.size(); ++o) {
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

void write_ambiguity_json(std::ostream& output, const TrackAmbiguity& event) {
    const json::FormatGuard format(output);
    output << "{\"local_id\": " << event.id << ", \"frame_index\": " << event.frame_index
           << ", \"start_sample\": " << event.start_sample << ", \"kind\": ";
    write_string(output, event.kind == TrackAmbiguityKind::Split ? "split" :
                         event.kind == TrackAmbiguityKind::Merge ? "merge" : "competing");
    output << ", \"parent_track_ids\": [";
    for (std::size_t i = 0; i < event.parent_track_ids.size(); ++i)
        output << (i ? ", " : "") << event.parent_track_ids[i];
    output << "], \"child_track_ids\": [";
    for (std::size_t i = 0; i < event.child_track_ids.size(); ++i)
        output << (i ? ", " : "") << event.child_track_ids[i];
    output << "]}";
}

AnalysisSettingsSnapshot::AnalysisSettingsSnapshot(const AnalysisReport& report)
    : frame_size_samples(report.frame_size_samples), hop_size_samples(report.hop_size_samples),
      support_threshold_dbfs(report.support_threshold_dbfs), minimum_prominence_db(report.minimum_prominence_db),
      tracker(report.tracker), welch(report.welch), peak_min_level_dbfs(report.peak_min_level_dbfs),
      peak_min_prominence_db(report.peak_min_prominence_db), max_peaks_per_frame(report.max_peaks_per_frame),
      max_harmonics(report.max_harmonics), spectrum_remove_mean(report.spectrum_remove_mean),
      peak_search(report.peak_search), harmonic_tolerance_hz(report.harmonic_tolerance_hz) {}

void write_analysis_settings_json(std::ostream& output, const AnalysisSettingsSnapshot& report) {
    const json::FormatGuard format(output);
    output << "{\n    \"window\": \"hann\",\n    \"frame_size_samples\": "
           << report.frame_size_samples << ",\n    \"hop_size_samples\": " << report.hop_size_samples
           << ",\n    \"support_threshold_dbfs\": ";
    write_number(output, report.support_threshold_dbfs);
    output << ",\n    \"minimum_prominence_db\": ";
    write_number(output, report.minimum_prominence_db);
    output << ",\n    \"welch_psd\": {\"window\": \"hann\", \"segment_size_samples\": "
           << report.welch.segment_size << ", \"hop_size_samples\": " << report.welch.hop_size
           << ", \"density_unit\": \"dBFS^2/Hz\", \"dc_removed\": " << (report.welch.remove_mean ? "true" : "false")
           << ", \"partial_segments\": \"excluded\", \"guard_bins\": " << report.peak_search.floor_guard_bins
           << ", \"radius_bins\": " << report.peak_search.floor_radius_bins
           << ", \"max_harmonics\": " << report.max_harmonics << ", \"minimum_prominence_db\": ";
    write_number(output, report.minimum_prominence_db);
    output << '}';
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
           << ", \"dc_removed\": " << (report.spectrum_remove_mean ? "true" : "false") << ", \"hop_fraction\": ";
    if (report.frame_size_samples != 0) write_number(output, static_cast<double>(report.hop_size_samples) / static_cast<double>(report.frame_size_samples));
    else output << "null";
    output << ", \"tail_window\": \"end-aligned\", \"uncertainty_status\": \"not_characterized\", "
              "\"width_method\": \"raw-bin half-power crossings; linear interpolation in dB\", "
              "\"competing_peak_rule\": \"other local maximum within 4 bins and 10 dB; deterministic, uncalibrated\", "
              "\"harmonic_tolerance_hz\": ";
    write_number(output, report.harmonic_tolerance_hz);
    output << ", \"peak_search\": {\"minimum_frequency_hz\": "; write_number(output, report.peak_search.minimum_frequency_hz);
    output << ", \"maximum_frequency_hz\": "; write_number(output, report.peak_search.maximum_frequency_hz);
    output << ", \"minimum_separation_hz\": "; write_number(output, report.peak_search.minimum_separation_hz);
    output << ", \"floor_guard_bins\": " << report.peak_search.floor_guard_bins
           << ", \"floor_radius_bins\": " << report.peak_search.floor_radius_bins << "}}\n  }";
}

void write_report_json(std::ostream& output, const AnalysisReport& report, const ReportArrays* arrays) {
    const json::FormatGuard format(output);
    output << std::setprecision(17)
           << "{\n  \"schema_version\": \"" << kReportSchemaVersion << "\",\n  \"application\": \"HumTrace\",\n  \"generator\": {\"application_version\": \"" << kApplicationVersion
           << "\", \"method_version\": \"" << kMethodVersion << "\"},\n  \"input\": {\n    \"path\": ";
    write_string(output, report.input_path);
    output << ",\n    \"sha256\": ";
    if (report.input_sha256.empty())
        output << "null";
    else
        write_string(output, report.input_sha256);
    output << ",\n    \"sha256_scope\": ";
    if (report.input_sha256.empty()) output << "null"; else write_string(output, "input_file_bytes");
    output << ",\n    \"bytes\": " << report.input_bytes
           << ",\n    \"sample_rate_hz\": " << report.sample_rate_hz
           << ",\n    \"channels\": " << report.channels
           << ",\n    \"bit_depth\": " << report.bit_depth
           << ",\n    \"format_code\": " << report.format_code
           << ",\n    \"duration_seconds\": ";
    write_number(output, report.duration_seconds);
    output << "\n  },\n  \"settings\": ";
    write_analysis_settings_json(output, AnalysisSettingsSnapshot(report));
    output << ",\n  \"warnings\": [";
    for (std::size_t i = 0; i < report.warnings.size(); ++i) {
        if (i != 0) output << ", ";
        write_string(output, report.warnings[i]);
    }
    output << "],\n  \"channels\": [";
    for (std::size_t channel_index = 0; channel_index < report.channel_results.size(); ++channel_index) {
        const auto& channel = report.channel_results[channel_index];
        if (channel_index != 0) output << ',';
        output << "\n    {\"channel_number\": " << channel.channel_number << ", \"frames\": [";
        if (arrays) arrays->frames(output, channel_index);
        else for (std::size_t frame_index = 0; frame_index < channel.frames.size(); ++frame_index) {
            const auto& frame = channel.frames[frame_index];
            if (frame_index != 0) output << ',';
            write_frame_json(output, frame, report.sample_rate_hz);
        }
        output << "], \"suppressed_tracks\": " << channel.suppressed_tracks << ", \"clipped_samples\": " << channel.clipped_samples
               << ", \"tracks\": [";
        if (arrays) arrays->tracks(output, channel_index);
        else for (std::size_t t = 0; t < channel.tracks.size(); ++t) {
            const auto& track = channel.tracks[t];
            if (t != 0) output << ", ";
            write_track_json(output, track);
        }
        output << "], \"ambiguities\": [";
        if (arrays && arrays->ambiguities) arrays->ambiguities(output, channel_index);
        else for (std::size_t i = 0; i < channel.ambiguities.size(); ++i) {
            if (i != 0) output << ", ";
            write_ambiguity_json(output, channel.ambiguities[i]);
        }
        output << "], \"psd\": ";
        if (channel.psd) write_psd_summary(output, *channel.psd);
        else output << "null";
        output << '}';
    }
    if (!report.channel_results.empty()) output << '\n' << "  ";
    output << "],\n  \"interpretation\": {\n    \"source_classification\": null,\n"
           << "    \"note\": \"Frequency and harmonic measurements do not prove a physical source.\",\n"
           << "    \"integrity_note\": \"SHA-256 identifies file bytes; it does not authenticate recording origin or prove an immutable analysis snapshot.\",\n"
           << "    \"tracking_note\": \"Track and ambiguity IDs are channel-local descriptive associations; ambiguity references may include segments suppressed by persistence filtering.\"\n  }\n}\n";
}

std::string serialize_report_json(const AnalysisReport& report) {
    std::ostringstream output;
    write_report_json(output, report);
    return output.str();
}

} // namespace humtrace
