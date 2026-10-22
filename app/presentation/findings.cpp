#include "findings.hpp"
#include "humtrace/music.hpp"

#include <algorithm>
#include <cstdio>

namespace humtrace::ui {
namespace {
std::string fmt(const char* format, double a, double b = 0.0, double c = 0.0) {
    char buffer[256];
    std::snprintf(buffer, sizeof buffer, format, a, b, c);
    return buffer;
}
} // namespace

std::vector<FindingRow> build_findings(const AnalysisReport& report) {
    std::vector<FindingRow> rows;
    const double rate = static_cast<double>(report.sample_rate_hz);
    for (const auto& channel : report.channel_results)
        for (const auto& track : channel.tracks) {
            if (track.observations.empty() || rate <= 0.0) continue;
            FindingRow row;
            row.channel = channel.channel_number;
            row.track_id = track.id;
            row.start_seconds = static_cast<double>(track.observations.front().start_sample) / rate;
            row.end_seconds = (static_cast<double>(track.observations.back().start_sample) +
                               static_cast<double>(report.frame_size_samples)) / rate;
            row.median_frequency_hz = track.median_frequency_hz;
            row.median_level_dbfs = track.median_level_dbfs;
            row.drift_hz_per_second = track.drift_hz_per_second;
            row.observed_frames = track.observed_frames;
            row.spanned_frames = track.observed_frames + track.missing_frames;
            row.first_frame = track.first_frame;
            row.last_frame = track.last_frame;
            rows.push_back(row);
        }
    std::stable_sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        if (a.channel != b.channel) return a.channel < b.channel;
        if (a.start_seconds != b.start_seconds) return a.start_seconds < b.start_seconds;
        return a.track_id < b.track_id;
    });
    return rows;
}

std::string describe_finding(const AnalysisReport& report, const FindingRow& row) {
    std::string text = "Observation: channel " + std::to_string(row.channel) + ", track " +
                       std::to_string(row.track_id) + " (analysis-local id)\n";
    text += "Track observed in " + std::to_string(row.observed_frames) + " of " +
            std::to_string(row.spanned_frames) + " frames.\n\n";
    text += "Measurements\n";
    text += fmt("  Median frequency: %.3f Hz\n", row.median_frequency_hz);
    if (row.median_frequency_hz > 0.0)
        text += "  Nearest equal-tempered note (A4 = 440 Hz): " +
                format_note(nearest_note(row.median_frequency_hz)) + " (a label, not evidence of musical content)\n";
    text += fmt("  Median peak level: %.2f dBFS (peak amplitude, not SPL)\n", row.median_level_dbfs);
    text += fmt("  Drift (least-squares): %.4f Hz/s\n", row.drift_hz_per_second);
    text += fmt("  Interval: %.3f s to %.3f s\n", row.start_seconds, row.end_seconds);
    text += fmt("  Frame coverage: %.0f%%\n", row.coverage() * 100.0);
    text += "\nHarmonic evidence over this track's frames (measurement only)\n";
    for (const auto& channel : report.channel_results) {
        if (channel.channel_number != row.channel) continue;
        std::size_t lo50 = 1000, hi50 = 0, lo60 = 1000, hi60 = 0, total = 0;
        for (std::size_t f = row.first_frame; f <= row.last_frame && f < channel.frames.size(); ++f) {
            const auto& frame = channel.frames[f];
            lo50 = std::min(lo50, frame.mains_50.supporting_harmonics);
            hi50 = std::max(hi50, frame.mains_50.supporting_harmonics);
            lo60 = std::min(lo60, frame.mains_60.supporting_harmonics);
            hi60 = std::max(hi60, frame.mains_60.supporting_harmonics);
            total = frame.mains_50.harmonics.size();
        }
        if (channel.psd) {
            const auto& psd = *channel.psd;
            text += "  Channel Welch PSD: " + std::to_string(psd.segments_averaged) + " complete segments\n";
            text += fmt("  Channel median density: %.2f dBFS^2/Hz\n", psd.broadband_median_dbfs2_per_hz);
            text += "  Prominent nominal 50/60 Hz harmonic bins: " +
                    std::to_string(psd.mains_50.prominent_harmonics) + "/" +
                    std::to_string(psd.mains_60.prominent_harmonics) + "\n";
        }
        bool measured = false, below_floor = false;
        double rms_lo = 0, rms_hi = 0, sample_peak = sample_level_floor_dbfs;
        for (std::size_t f = row.first_frame; f <= row.last_frame && f < channel.frames.size(); ++f) {
            const auto& levels = channel.frames[f].levels;
            if (!levels) continue;
            if (!measured) rms_lo = rms_hi = levels->rms_dbfs;
            measured = true;
            rms_lo = std::min(rms_lo, levels->rms_dbfs);
            rms_hi = std::max(rms_hi, levels->rms_dbfs);
            sample_peak = std::max(sample_peak, levels->sample_peak_dbfs);
            below_floor = below_floor || levels->rms_below_numeric_floor;
        }
        if (measured) {
            text += fmt("  Frame RMS range (includes DC, reference amplitude 1.0): %.2f to %.2f dBFS\n", rms_lo, rms_hi);
            text += fmt("  Maximum raw sample peak across those frames: %.2f dBFS (not true peak)\n", sample_peak);
            if (below_floor) text += "  Some RMS values are below the -300 dBFS numeric reporting floor.\n";
        }
        text += "  50 Hz harmonic bins above threshold: " + std::to_string(lo50 > hi50 ? 0 : lo50) + " to " +
                std::to_string(hi50) + " of " + std::to_string(total) + "\n";
        text += "  60 Hz harmonic bins above threshold: " + std::to_string(lo60 > hi60 ? 0 : lo60) + " to " +
                std::to_string(hi60) + " of " + std::to_string(total) + "\n";
    }
    text += fmt("  Thresholds: level >= %.0f dBFS and >= %.0f dB over local floor\n",
                report.support_threshold_dbfs, report.minimum_prominence_db);
    text += "\nSettings\n";
    text += "  Hann window, " + std::to_string(report.frame_size_samples) + " samples, hop " +
            std::to_string(report.hop_size_samples) + " samples, " +
            std::to_string(report.sample_rate_hz) + " Hz\n";
    text += fmt("  Track match radius: max(%.2f Hz, ", report.tracker.fixed_hz_tolerance);
    text += fmt("%.2f x bin spacing %.3f Hz)\n", report.tracker.bin_fraction, report.tracker.bin_spacing_hz);
    text += "\nLimitations\n"
            "  A frequency pattern alone does not identify a device, ground loop, location,\n"
            "  recording date, or authenticity. Persistence labels are not validated against\n"
            "  a labeled corpus; this is a descriptive observation.\n";
    return text;
}

std::string summarize_report(const AnalysisReport& report, const std::vector<FindingRow>& rows) {
    std::string text = "Recording of " + fmt("%.1f", report.duration_seconds) + " seconds, " +
           std::to_string(report.channels) + " channel(s) at " + std::to_string(report.sample_rate_hz) +
           " Hz. " + std::to_string(rows.size()) + " track(s) measured. Measurements only; no source attribution.";
    std::size_t measured_channels = 0;
    double floor_lo = 0, floor_hi = 0;
    for (const auto& channel : report.channel_results) {
        if (!channel.psd) continue;
        const auto floor = channel.psd->broadband_median_dbfs2_per_hz;
        if (measured_channels++ == 0) floor_lo = floor_hi = floor;
        floor_lo = std::min(floor_lo, floor);
        floor_hi = std::max(floor_hi, floor);
    }
    text += " Welch PSD: " + std::to_string(measured_channels) + "/" +
            std::to_string(report.channel_results.size()) + " channels with complete segments.";
    if (measured_channels > 0)
        text += fmt(" Per-channel median density range %.2f to %.2f dBFS^2/Hz.", floor_lo, floor_hi);
    return text;
}

} // namespace humtrace::ui
