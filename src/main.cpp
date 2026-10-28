#include "humtrace/analysis.hpp"
#include "humtrace/compare.hpp"
#include "humtrace/version.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::atomic<bool> g_cancel{false};
extern "C" void on_interrupt(int) { g_cancel.store(true); }

void usage() {
    std::cerr << "Usage: humtrace <input.wav> [--json <report.json>] [--frame-size N]\n"
                 "                [--threshold dBFS] [--prominence dB] [--no-progress]\n"
                 "       humtrace --compare <a.wav> <b.wav> [...] [options]\n"
                 "       humtrace --version\n";
}

double parse_number(const std::string& text, const char* name) {
    std::size_t used = 0;
    double value = 0.0;
    try { value = std::stod(text, &used); } catch (const std::exception&) { used = 0; }
    if (used != text.size() || used == 0)
        throw std::runtime_error(std::string("invalid number for ") + name);
    return value;
}

struct Cli {
    std::vector<std::string> inputs;
    std::string json_path;
    bool compare{false};
    bool progress{true};
    humtrace::AnalysisOptions options;
};

Cli parse(int argc, char** argv) {
    Cli cli;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string(name) + " needs a value");
            return argv[++i];
        };
        if (arg == "--json") cli.json_path = value("--json");
        else if (arg == "--compare") cli.compare = true;
        else if (arg == "--no-progress") cli.progress = false;
        else if (arg == "--frame-size") {
            const auto v = parse_number(value("--frame-size"), "--frame-size");
            if (v < 0 || v > 1e9) throw std::runtime_error("invalid frame size");
            cli.options.frame_size_samples = static_cast<std::size_t>(v);
        } else if (arg == "--threshold") cli.options.support_threshold_dbfs = parse_number(value("--threshold"), "--threshold");
        else if (arg == "--prominence") cli.options.minimum_prominence_db = parse_number(value("--prominence"), "--prominence");
        else if (arg.rfind("--", 0) == 0) throw std::runtime_error("unknown option " + arg);
        else cli.inputs.push_back(arg);
    }
    if (cli.inputs.empty() || (!cli.compare && cli.inputs.size() != 1) || (cli.compare && cli.inputs.size() < 2))
        throw std::invalid_argument("bad arguments");
    if (cli.compare && !cli.json_path.empty())
        throw std::runtime_error("--json is not supported with --compare");
    return cli;
}

void print_report(const humtrace::AnalysisReport& r) {
    std::cout << std::fixed << std::setprecision(3) << "HumTrace analysis ("
              << static_cast<double>(r.frame_size_samples) / r.sample_rate_hz << " s per frame)\n"
              << "Sample rate: " << r.sample_rate_hz << " Hz\nChannels: " << r.channels
              << "\nBit depth: " << r.bit_depth << "\nDuration: " << r.duration_seconds << " s\n"
              << "FFT bins: " << r.frame_size_samples / 2 + 1 << " ("
              << static_cast<double>(r.sample_rate_hz) / static_cast<double>(r.frame_size_samples)
              << " Hz/bin)\nTimeline hop: "
              << static_cast<double>(r.hop_size_samples) / r.sample_rate_hz << " s\n"
              << "SHA-256: " << r.input_sha256 << '\n';
    for (const auto& w : r.warnings) std::cout << "Warning: " << w << '\n';
    for (const auto& channel : r.channel_results) {
        std::cout << "Channel " << channel.channel_number << " measured frames: " << channel.frames.size() << '\n';
        for (const auto& frame : channel.frames) {
            const auto& peak = frame.strongest_peak;
            std::cout << "  " << static_cast<double>(frame.start_sample) / r.sample_rate_hz << " s: ";
            if (peak.frequency_hz == 0.0) std::cout << "no non-DC energy";
            else std::cout << "strongest non-DC component " << peak.frequency_hz << " Hz, " << peak.level_dbfs << " dBFS";
            std::cout << "; 50 Hz bins over " << r.support_threshold_dbfs << " dBFS and "
                      << r.minimum_prominence_db << " dB local prominence: "
                      << frame.mains_50.supporting_harmonics << '/' << frame.mains_50.harmonics.size()
                      << "; 60 Hz: " << frame.mains_60.supporting_harmonics << '/'
                      << frame.mains_60.harmonics.size() << '\n';
        }
    }
    std::cout << "Interpretation: spectral components are measurements, not source attribution.\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "humtrace " << humtrace::kVersion << '\n';
        return 0;
    }
    try {
        Cli cli;
        try { cli = parse(argc, argv); }
        catch (const std::invalid_argument&) { usage(); return 2; }
        std::signal(SIGINT, on_interrupt);
        std::signal(SIGTERM, on_interrupt);
        cli.options.cancel = &g_cancel;
        if (cli.progress)
            cli.options.progress = [](double f) {
                std::cerr << "\rProgress: " << static_cast<int>(f * 100) << "%" << std::flush;
            };
        std::vector<humtrace::FileSummary> summaries;
        for (const auto& path : cli.inputs) {
            const auto report = humtrace::analyze_wav_file(path, cli.options);
            if (cli.progress) std::cerr << '\n';
            if (cli.compare) { summaries.push_back(humtrace::summarize(report)); continue; }
            print_report(report);
            if (!cli.json_path.empty()) {
                humtrace::write_file_safely(cli.json_path, humtrace::serialize_report_json(report), path);
                std::cout << "JSON report saved: " << cli.json_path << '\n';
            }
        }
        if (cli.compare) std::cout << humtrace::format_comparison(summaries);
    } catch (const humtrace::AnalysisCancelled&) {
        std::cerr << "\nHumTrace: cancelled\n";
        return 130;
    } catch (const std::exception& error) {
        std::cerr << "\nHumTrace: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
