#include "humtrace/spectrum.hpp"
#include "humtrace/report.hpp"
#include "humtrace/tracks.hpp"
#include "humtrace/wav.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr double support_threshold_dbfs = -60.0;
constexpr double minimum_prominence_db = 10.0;
constexpr std::size_t max_harmonics = 10;
constexpr std::size_t max_peaks_per_frame_limit = 64;

struct Options {
    std::string input_path;
    std::string json_path;
    bool write_json = false;
    humtrace::PeakPickingSettings peak_picking;
    humtrace::TrackSettings tracks;
};

void usage() {
    std::cerr << "Usage: humtrace <input.wav> [--json <report.json>]\n"
              << "       [--peaks-per-frame N] [--track-tolerance-hz HZ]\n"
              << "       [--track-max-gap-frames N] [--track-min-frames N]\n";
}

std::size_t parse_count(const std::string& text, const char* name, std::size_t minimum,
                        std::size_t maximum) {
    std::size_t consumed = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(text, &consumed);
    } catch (const std::exception&) {
        consumed = 0;
    }
    if (consumed != text.size() || value < minimum || value > maximum)
        throw std::invalid_argument(std::string(name) + " must be an integer from " +
                                    std::to_string(minimum) + " to " + std::to_string(maximum));
    return static_cast<std::size_t>(value);
}

double parse_hz(const std::string& text, const char* name) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception&) {
        consumed = 0;
    }
    if (consumed != text.size() || !std::isfinite(value) || value < 0.0)
        throw std::invalid_argument(std::string(name) + " must be a finite non-negative number");
    return value;
}

Options parse_options(int argc, char** argv) {
    if (argc < 2)
        throw std::invalid_argument("missing input file");
    Options options;
    options.input_path = argv[1];
    options.peak_picking.support_threshold_dbfs = support_threshold_dbfs;
    options.peak_picking.minimum_prominence_db = minimum_prominence_db;
    for (int i = 2; i < argc; ++i) {
        const std::string flag = argv[i];
        if (i + 1 >= argc)
            throw std::invalid_argument("option " + flag + " requires a value");
        const std::string value = argv[++i];
        if (flag == "--json") {
            options.json_path = value;
            options.write_json = true;
        } else if (flag == "--peaks-per-frame") {
            options.peak_picking.max_peaks = parse_count(value, "--peaks-per-frame", 1,
                                                         max_peaks_per_frame_limit);
        } else if (flag == "--track-tolerance-hz") {
            options.tracks.frequency_tolerance_hz = parse_hz(value, "--track-tolerance-hz");
        } else if (flag == "--track-max-gap-frames") {
            options.tracks.max_gap_frames = parse_count(value, "--track-max-gap-frames", 0, 1000);
        } else if (flag == "--track-min-frames") {
            options.tracks.min_observed_frames = parse_count(value, "--track-min-frames", 1, 1000000);
        } else {
            throw std::invalid_argument("unknown option " + flag);
        }
    }
    return options;
}
} // namespace

int main(int argc, char** argv) {
    Options options;
    try {
        options = parse_options(argc, argv);
    } catch (const std::invalid_argument& error) {
        std::cerr << "HumTrace: " << error.what() << '\n';
        usage();
        return 2;
    }
    try {
        std::ifstream input(options.input_path, std::ios::binary);
        if (!input)
            throw std::runtime_error("could not open input file");
        humtrace::WavStreamReader reader(input);
        const auto& metadata = reader.metadata();
        const auto frames = metadata.frame_count;
        if (frames < 4)
            throw std::runtime_error("audio is too short to analyze");
        std::size_t window_size = 1;
        const auto frame_limit = std::min<std::uint64_t>(
            frames, std::max<std::uint64_t>(4, std::min<std::uint64_t>(metadata.sample_rate_hz, 65536)));
        while (window_size <= frame_limit / 2)
            window_size *= 2;
        const std::size_t hop_size = window_size / 2;
        constexpr std::size_t max_interleaved_window_samples = 16 * 1024 * 1024;
        if (window_size > max_interleaved_window_samples / metadata.channels)
            throw std::runtime_error("too many channels for the selected analysis window");
        const auto bin_spacing = static_cast<double>(metadata.sample_rate_hz) /
                                 static_cast<double>(window_size);
        humtrace::AnalysisReport report;
        report.input_path = options.input_path;
        report.sample_rate_hz = metadata.sample_rate_hz;
        report.channels = metadata.channels;
        report.bit_depth = metadata.bit_depth;
        report.format_code = metadata.format_code;
        report.duration_seconds = static_cast<double>(frames) / metadata.sample_rate_hz;
        report.frame_size_samples = window_size;
        report.hop_size_samples = hop_size;
        report.support_threshold_dbfs = support_threshold_dbfs;
        report.minimum_prominence_db = minimum_prominence_db;
        report.peak_picking = options.peak_picking;
        report.track_settings = options.tracks;

        std::cout << std::fixed << std::setprecision(3)
                  << "HumTrace analysis (" << static_cast<double>(window_size) /
                         metadata.sample_rate_hz << " s per frame)\n"
                  << "Sample rate: " << metadata.sample_rate_hz << " Hz\n"
                  << "Channels: " << metadata.channels << "\n"
                  << "Bit depth: " << metadata.bit_depth << "\n"
                  << "Duration: " << static_cast<double>(frames) / metadata.sample_rate_hz << " s\n"
                  << "FFT bins: " << window_size / 2 + 1 << " (" << bin_spacing << " Hz/bin)\n"
                  << "Timeline hop: " << static_cast<double>(hop_size) / metadata.sample_rate_hz << " s\n";

        std::vector<humtrace::ReportChannel> channel_results(metadata.channels);
        std::vector<std::vector<humtrace::PeakFrame>> peak_timelines(metadata.channels);
        for (std::uint16_t channel = 0; channel < metadata.channels; ++channel)
            channel_results[channel].channel_number = static_cast<std::uint16_t>(channel + 1);
        auto interleaved_window = reader.read_frames(window_size);
        const auto samples_per_window = window_size * metadata.channels;
        if (interleaved_window.size() != samples_per_window)
            throw std::runtime_error("could not read the first complete analysis window");
        std::uint64_t window_start = 0;
        while (true) {
            for (std::uint16_t channel = 0; channel < metadata.channels; ++channel) {
                std::vector<double> samples(window_size);
                for (std::size_t frame = 0; frame < window_size; ++frame)
                    samples[frame] = interleaved_window[frame * metadata.channels + channel];
                // One spectrum per window feeds both the dominant-bin frame and the peak list.
                const auto spectrum = humtrace::analyze_spectrum(samples, metadata.sample_rate_hz);
                channel_results[channel].frames.push_back({window_start,
                    humtrace::strongest_peak(spectrum),
                    humtrace::measure_harmonics(spectrum, 50.0, support_threshold_dbfs, max_harmonics,
                                                minimum_prominence_db),
                    humtrace::measure_harmonics(spectrum, 60.0, support_threshold_dbfs, max_harmonics,
                                                minimum_prominence_db)});
                peak_timelines[channel].push_back({window_start,
                    humtrace::find_spectral_peaks(spectrum, options.peak_picking)});
            }
            if (window_start + hop_size + window_size > frames)
                break;
            const auto retained_samples = hop_size * metadata.channels;
            std::move(interleaved_window.begin() + static_cast<std::ptrdiff_t>(retained_samples),
                      interleaved_window.end(), interleaved_window.begin());
            auto next_samples = reader.read_frames(hop_size);
            if (next_samples.size() != retained_samples)
                break;
            std::copy(next_samples.begin(), next_samples.end(),
                      interleaved_window.end() - static_cast<std::ptrdiff_t>(next_samples.size()));
            window_start += hop_size;
        }

        for (std::uint16_t channel = 0; channel < metadata.channels; ++channel) {
            auto& channel_result = channel_results[channel];
            channel_result.tracks = humtrace::associate_peak_tracks(
                peak_timelines[channel], metadata.sample_rate_hz, options.tracks,
                channel_result.channel_number);
        }

        for (const auto& channel : channel_results) {
            const auto& timeline = channel.frames;
            std::cout << "Channel " << channel.channel_number << " measured frames: " << timeline.size() << '\n';
            for (const auto& frame : timeline) {
                const auto start_seconds = static_cast<double>(frame.start_sample) / metadata.sample_rate_hz;
                const auto& peak = frame.strongest_peak;
                std::cout << "  " << start_seconds << " s: ";
                if (peak.frequency_hz == 0.0)
                    std::cout << "no non-DC energy";
                else
                    std::cout << "strongest non-DC component " << peak.frequency_hz << " Hz, "
                              << peak.level_dbfs << " dBFS";
                std::cout << "; "
                          << "50 Hz bins over -60 dBFS and 10 dB local prominence: "
                          << frame.mains_50.supporting_harmonics << '/' << frame.mains_50.harmonics.size()
                          << "; 60 Hz: " << frame.mains_60.supporting_harmonics << '/'
                          << frame.mains_60.harmonics.size() << '\n';
            }
            std::cout << "Channel " << channel.channel_number << " peak tracks: " << channel.tracks.size()
                      << " (peaks per frame " << options.peak_picking.max_peaks
                      << ", tolerance " << options.tracks.frequency_tolerance_hz << " Hz, max gap "
                      << options.tracks.max_gap_frames << " frames, min " << options.tracks.min_observed_frames
                      << " observed frames)\n";
            for (const auto& track : channel.tracks) {
                const auto start_seconds = static_cast<double>(track.start_sample) / metadata.sample_rate_hz;
                const auto end_seconds = static_cast<double>(track.end_sample) / metadata.sample_rate_hz;
                std::cout << "  Track " << track.track_number << ": " << start_seconds << "-" << end_seconds
                          << " s, " << track.observations.size() << " observed, " << track.missing_frames
                          << " missing; frequency mean " << track.mean_frequency_hz << " Hz (min "
                          << track.min_frequency_hz << ", max " << track.max_frequency_hz
                          << "), drift " << track.frequency_drift_hz << " Hz";
                if (std::isfinite(track.drift_rate_hz_per_second))
                    std::cout << " (" << track.drift_rate_hz_per_second << " Hz/s)";
                std::cout << "; level mean " << track.mean_level_dbfs << " dBFS, max "
                          << track.max_level_dbfs << " dBFS\n";
            }
            report.channel_results.push_back(channel);
        }
        std::cout << "Interpretation: spectral components and tracks are measurements, not source attribution.\n";
        if (options.write_json) {
            const auto json = humtrace::serialize_report_json(report);
            std::ofstream report_file(options.json_path, std::ios::binary | std::ios::trunc);
            if (!report_file)
                throw std::runtime_error("could not create JSON report");
            report_file << json;
            if (!report_file)
                throw std::runtime_error("failed while writing JSON report");
            std::cout << "JSON report saved: " << options.json_path << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "HumTrace: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
