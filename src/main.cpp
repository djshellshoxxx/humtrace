#include "humtrace/compare.hpp"
#include "humtrace/music.hpp"
#include "humtrace/report.hpp"
#include "humtrace/runner.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> cancel_requested{false};
void on_interrupt(int) { cancel_requested.store(true); }

constexpr const char* usage =
    "Usage: humtrace <input.wav> [--json <report.json>] [--compare <reference.wav>]\n"
    "                [--compare-tolerance <Hz>] [--a4 <Hz>] [--full-path] [--progress]\n";

struct CliArguments {
    std::string input;
    std::string json_path;
    std::string compare_path;
    double compare_tolerance_hz{1.0};
    double a4_hz{440.0};
    bool full_path{false};
    bool progress{false};
};

double parse_positive(const std::string& text, const char* name) {
    std::size_t consumed = 0;
    double value = 0.0;
    try { value = std::stod(text, &consumed); } catch (const std::exception&) { consumed = 0; }
    if (consumed != text.size() || !(value > 0.0) || value > 1e9)
        throw std::invalid_argument(std::string(name) + " must be a positive number");
    return value;
}

CliArguments parse_arguments(int argc, char** argv) {
    CliArguments args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(arg + " needs a value");
            return argv[++i];
        };
        if (arg == "--json") args.json_path = value();
        else if (arg == "--compare") args.compare_path = value();
        else if (arg == "--compare-tolerance") args.compare_tolerance_hz = parse_positive(value(), "--compare-tolerance");
        else if (arg == "--a4") args.a4_hz = parse_positive(value(), "--a4");
        else if (arg == "--full-path") args.full_path = true;
        else if (arg == "--progress") args.progress = true;
        else if (!arg.empty() && arg[0] == '-') throw std::invalid_argument("unknown option " + arg);
        else if (args.input.empty()) args.input = arg;
        else throw std::invalid_argument("only one input file is accepted");
    }
    if (args.input.empty())
        throw std::invalid_argument("missing input file");
    return args;
}

void print_track_summary(const humtrace::AnalysisReport& report, double a4_hz) {
    for (const auto& channel : report.channel_results) {
        std::cout << "Channel " << channel.channel_number << " persistent tracks: " << channel.tracks.size() << '\n';
        for (const auto& track : channel.tracks) {
            const auto note = humtrace::format_note(humtrace::describe_note(track.median_frequency_hz, a4_hz));
            std::cout << "  track " << track.id << ": " << track.median_frequency_hz << " Hz (" << note << "), "
                      << track.median_level_dbfs << " dBFS, observed " << track.observations.size()
                      << " frames, " << track.missing_frames << " gap frames, drift "
                      << track.drift_hz_per_second << " Hz/s, "
                      << static_cast<double>(track.first_sample) / report.sample_rate_hz << '-'
                      << static_cast<double>(track.last_sample) / report.sample_rate_hz << " s\n";
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    CliArguments args;
    try {
        args = parse_arguments(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "HumTrace: " << error.what() << '\n' << usage;
        return 2;
    }
    std::signal(SIGINT, on_interrupt);
    std::signal(SIGTERM, on_interrupt);
    try {
        humtrace::AnalysisOptions options;
        options.a4_reference_hz = args.a4_hz;
        options.record_full_path = args.full_path;
        options.cancel = &cancel_requested;
        if (args.progress)
            options.progress = [](double fraction) {
                std::cerr << "\rprogress: " << static_cast<int>(fraction * 100.0) << "%" << std::flush;
            };
        auto report = humtrace::analyze_wav_file(args.input, options);
        if (args.progress)
            std::cerr << '\n';
        const auto frames_per_channel = report.channel_results.empty() ? 0 : report.channel_results[0].frames.size();

        std::cout << std::fixed << std::setprecision(3)
                  << "HumTrace analysis (" << static_cast<double>(report.frame_size_samples) /
                         report.sample_rate_hz << " s per frame)\n"
                  << "Sample rate: " << report.sample_rate_hz << " Hz\n"
                  << "Channels: " << report.channels << "\n"
                  << "Bit depth: " << report.bit_depth << "\n"
                  << "Duration: " << report.duration_seconds << " s\n"
                  << "FFT bins: " << report.frame_size_samples / 2 + 1 << " ("
                  << static_cast<double>(report.sample_rate_hz) / static_cast<double>(report.frame_size_samples)
                  << " Hz/bin)\n"
                  << "Timeline hop: " << static_cast<double>(report.hop_size_samples) / report.sample_rate_hz << " s\n"
                  << "SHA-256: " << report.input_sha256 << " (" << report.input_size_bytes << " bytes)\n";
        if (!report.input_unchanged_during_analysis)
            std::cout << "WARNING: input file changed during analysis; results are not reliable.\n";
        for (const auto& channel : report.channel_results) {
            std::cout << "Channel " << channel.channel_number << " measured frames: " << frames_per_channel << '\n';
            for (const auto& frame : channel.frames) {
                const auto start_seconds = static_cast<double>(frame.start_sample) / report.sample_rate_hz;
                const auto& peak = frame.strongest_peak;
                std::cout << "  " << start_seconds << " s: ";
                if (peak.frequency_hz == 0.0)
                    std::cout << "no non-DC energy";
                else
                    std::cout << "strongest non-DC component " << peak.frequency_hz << " Hz ("
                              << humtrace::format_note(humtrace::describe_note(peak.frequency_hz, args.a4_hz))
                              << "), " << peak.level_dbfs << " dBFS";
                std::cout << "; "
                          << "50 Hz bins over -60 dBFS and 10 dB local prominence: "
                          << frame.mains_50.supporting_harmonics << '/' << frame.mains_50.harmonics.size()
                          << "; 60 Hz: " << frame.mains_60.supporting_harmonics << '/'
                          << frame.mains_60.harmonics.size() << '\n';
            }
        }
        print_track_summary(report, args.a4_hz);

        if (!args.compare_path.empty()) {
            const auto reference = humtrace::analyze_wav_file(args.compare_path, options);
            if (reference.channels != report.channels)
                throw std::runtime_error("comparison requires equal channel counts (channels are paired by index)");
            humtrace::ReportComparison comparison;
            comparison.reference_name = reference.input_path;
            comparison.reference_sha256 = reference.input_sha256;
            comparison.reference_size_bytes = reference.input_size_bytes;
            comparison.reference_sample_rate_hz = reference.sample_rate_hz;
            std::cout << "Comparison with " << reference.input_path << " (no alignment, no resampling; channels paired by index; tolerance "
                      << args.compare_tolerance_hz << " Hz)\n";
            if (reference.sample_rate_hz != report.sample_rate_hz)
                std::cout << "  note: sample rates differ (" << report.sample_rate_hz << " vs "
                          << reference.sample_rate_hz << " Hz); frequency resolution differs\n";
            for (std::size_t c = 0; c < report.channel_results.size(); ++c) {
                auto result = humtrace::compare_tracks(report.channel_results[c].tracks,
                                                       reference.channel_results[c].tracks,
                                                       args.compare_tolerance_hz);
                std::cout << "  channel " << c + 1 << ": " << result.matches.size() << " matched, "
                          << result.unmatched_a.size() << " only in input, "
                          << result.unmatched_b.size() << " only in reference\n";
                for (const auto& m : result.matches)
                    std::cout << "    track " << m.track_a_id << " ~ ref " << m.track_b_id << ": delta "
                              << m.delta_frequency_hz << " Hz (" << m.delta_cents << " cents), "
                              << m.delta_level_db << " dB\n";
                comparison.channels.push_back({static_cast<std::uint16_t>(c + 1), std::move(result)});
            }
            std::cout << "Similarity of tracks does not show a shared source, device, or location.\n";
            report.comparison = std::move(comparison);
        }
        std::cout << "Interpretation: spectral components are measurements, not source attribution.\n";
        if (!args.json_path.empty()) {
            humtrace::write_file_atomically(args.json_path, humtrace::serialize_report_json(report));
            std::cout << "JSON report saved: " << args.json_path << '\n';
        }
    } catch (const humtrace::AnalysisCancelled&) {
        std::cerr << "HumTrace: cancelled; no report written\n";
        return 130;
    } catch (const std::exception& error) {
        std::cerr << "HumTrace: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
