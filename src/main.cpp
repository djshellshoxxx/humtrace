#include "humtrace/compare.hpp"
#include "humtrace/session.hpp"

#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::atomic<bool> g_cancel{false};
extern "C" void on_interrupt(int) { g_cancel = true; }

void print_summary(const humtrace::AnalysisReport& report) {
    const auto rate = static_cast<double>(report.sample_rate_hz);
    const auto window = static_cast<double>(report.frame_size_samples);
    std::cout << std::fixed << std::setprecision(3)
              << "HumTrace analysis (" << window / rate << " s per frame)\n"
              << "Sample rate: " << report.sample_rate_hz << " Hz\n"
              << "Channels: " << report.channels << "\n"
              << "Bit depth: " << report.bit_depth << "\n"
              << "Duration: " << report.duration_seconds << " s\n"
              << "FFT bins: " << report.frame_size_samples / 2 + 1 << " ("
              << rate / window << " Hz/bin)\n"
              << "Timeline hop: " << static_cast<double>(report.hop_size_samples) / rate << " s\n"
              << "Input SHA-256: " << report.input_sha256 << '\n';
    for (const auto& channel : report.channel_results) {
        std::cout << "Channel " << channel.channel_number << " measured frames: "
                  << channel.frames.size() << '\n';
        for (const auto& frame : channel.frames) {
            const auto& peak = frame.strongest_peak;
            std::cout << "  " << static_cast<double>(frame.start_sample) / rate << " s: ";
            if (peak.frequency_hz == 0.0)
                std::cout << "no non-DC energy";
            else
                std::cout << "strongest non-DC component " << peak.frequency_hz << " Hz, "
                          << peak.level_dbfs << " dBFS";
            std::cout << "; 50 Hz bins over " << report.support_threshold_dbfs << " dBFS and "
                      << report.minimum_prominence_db << " dB local prominence: "
                      << frame.mains_50.supporting_harmonics << '/' << frame.mains_50.harmonics.size()
                      << "; 60 Hz: " << frame.mains_60.supporting_harmonics << '/'
                      << frame.mains_60.harmonics.size() << '\n';
        }
        for (const auto& track : channel.tracks)
            std::cout << "  Track " << track.id << ": observed in " << track.observed_frames << " of "
                      << track.observed_frames + track.missing_frames << " frames, median "
                      << track.median_frequency_hz << " Hz, drift " << track.drift_hz_per_second
                      << " Hz/s\n";
    }
    std::cout << "Interpretation: spectral components are measurements, not source attribution.\n";
}

humtrace::AnalysisReport run(const std::string& path) {
    humtrace::SessionCallbacks callbacks;
    callbacks.cancelled = [] { return g_cancel.load(); };
    return humtrace::analyze_file(path, {}, callbacks, false).report;
}

int usage() {
    std::cerr << "Usage: humtrace <input.wav> [--json <report.json>]\n"
                 "       humtrace compare <a.wav> <b.wav> [--tolerance <Hz>] [--json <comparison.json>]\n";
    return 2;
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_interrupt);
    const std::vector<std::string> args(argv + 1, argv + argc);
    try {
        if (!args.empty() && args[0] == "compare") {
            if (args.size() < 3)
                return usage();
            humtrace::CompareSettings settings;
            std::string json_path;
            for (std::size_t i = 3; i < args.size(); i += 2) {
                if (i + 1 >= args.size())
                    return usage();
                if (args[i] == "--json") json_path = args[i + 1];
                else if (args[i] == "--tolerance") settings.frequency_tolerance_hz = std::stod(args[i + 1]);
                else return usage();
            }
            const auto comparison = humtrace::compare_reports(run(args[1]), run(args[2]), settings);
            std::cout << "Matched tracks: " << comparison.matches.size() << ", unmatched in A: "
                      << comparison.unmatched_a.size() << ", unmatched in B: "
                      << comparison.unmatched_b.size() << '\n';
            for (const auto& m : comparison.matches)
                std::cout << "  ch" << m.a.channel << " " << m.a.median_frequency_hz << " Hz <-> ch"
                          << m.b.channel << " " << m.b.median_frequency_hz << " Hz (delta "
                          << m.delta_frequency_hz << " Hz, " << m.delta_level_db << " dB)\n";
            for (const auto& w : comparison.warnings)
                std::cout << "Warning: " << w << '\n';
            std::cout << "Interpretation: similar frequencies do not show a common device or environment.\n";
            if (!json_path.empty()) {
                humtrace::write_file_atomically(json_path, humtrace::serialize_comparison_json(comparison));
                std::cout << "JSON comparison saved: " << json_path << '\n';
            }
            return 0;
        }
        if (args.size() != 1 && !(args.size() == 3 && args[1] == "--json"))
            return usage();
        const auto report = run(args[0]);
        print_summary(report);
        if (args.size() == 3) {
            humtrace::write_file_atomically(args[2], humtrace::serialize_report_json(report));
            std::cout << "JSON report saved: " << args[2] << '\n';
        }
    } catch (const humtrace::AnalysisCancelled&) {
        std::cerr << "HumTrace: analysis cancelled; no output written\n";
        return 130;
    } catch (const std::exception& error) {
        std::cerr << "HumTrace: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
