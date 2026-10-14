#include "humtrace/compare.hpp"
#include "humtrace/session.hpp"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::atomic<bool> g_cancel{false};
extern "C" void on_interrupt(int) { g_cancel = true; }

void print_summary(const humtrace::AnalysisReport& report, bool quiet) {
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
        if (!quiet)
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
    for (const auto& w : report.warnings)
        std::cout << "Warning: " << w << '\n';
    std::cout << "Interpretation: spectral components are measurements, not source attribution.\n";
}

humtrace::AnalysisReport run(const std::string& path) {
    humtrace::SessionCallbacks callbacks;
    callbacks.cancelled = [] { return g_cancel.load(); };
    return humtrace::analyze_file(path, {}, callbacks, false).report;
}

int usage() {
    std::cerr << "Usage: humtrace <input.wav> [--json <report.json>] [--quiet] [--full-path] [--force]\n"
                 "       humtrace compare <a.wav> <b.wav> [--tolerance <Hz>] [--map <a:b[,a:b...]>]\n"
                 "                        [--json <comparison.json>] [--full-path] [--force]\n"
                 "  --quiet      omit the per-frame lines\n"
                 "  --full-path  store input paths as typed (default: file name only)\n"
                 "  --force      replace an existing output file (default: refuse)\n";
    return 2;
}

struct Options {
    std::string json_path;
    bool quiet{false};
    bool full_path{false};
    bool force{false};
    humtrace::CompareSettings compare;
};

// Parses options after the positional arguments; returns false on a usage error.
bool parse_options(const std::vector<std::string>& args, std::size_t first, bool allow_compare, Options& out) {
    for (std::size_t i = first; i < args.size(); ++i) {
        const auto& a = args[i];
        auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size()) throw std::invalid_argument(a + " needs a value");
            return args[++i];
        };
        if (a == "--json") out.json_path = value();
        else if (a == "--quiet") out.quiet = true;
        else if (a == "--full-path") out.full_path = true;
        else if (a == "--force") out.force = true;
        else if (allow_compare && a == "--tolerance") {
            const auto& v = value();
            std::size_t used = 0;
            out.compare.frequency_tolerance_hz = std::stod(v, &used);
            if (used != v.size()) throw std::invalid_argument("--tolerance needs a number");
        } else if (allow_compare && a == "--map") {
            std::stringstream list(value());
            std::string pair;
            while (std::getline(list, pair, ',')) {
                const auto colon = pair.find(':');
                std::size_t ua = 0, ub = 0;
                if (colon == std::string::npos) throw std::invalid_argument("--map entries look like 1:2");
                const auto left = std::stoul(pair.substr(0, colon), &ua);
                const auto right = std::stoul(pair.substr(colon + 1), &ub);
                if (ua != colon || ub != pair.size() - colon - 1 || left == 0 || right == 0 || left > 65535 || right > 65535)
                    throw std::invalid_argument("--map entries look like 1:2");
                out.compare.channel_map.emplace_back(static_cast<std::uint16_t>(left), static_cast<std::uint16_t>(right));
            }
        } else {
            return false;
        }
    }
    return true;
}

// Refuse to overwrite an input, and (unless --force) any existing file.
void check_output(const std::string& output, const std::vector<std::string>& inputs, bool force) {
    for (const auto& input : inputs) humtrace::require_distinct_paths(output, input);
    if (!force && std::filesystem::exists(humtrace::path_from_utf8(output)))
        throw std::runtime_error("output file already exists; use --force to replace it");
}

std::string display_path(const std::string& path, bool full) {
    return full ? path : humtrace::path_from_utf8(path).filename().string();
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_interrupt);
    const std::vector<std::string> args(argv + 1, argv + argc);
    try {
        if (!args.empty() && args[0] == "compare") {
            Options options;
            if (args.size() < 3 || !parse_options(args, 3, true, options)) return usage();
            if (!options.json_path.empty()) check_output(options.json_path, {args[1], args[2]}, options.force);
            auto a = run(args[1]);
            auto b = run(args[2]);
            a.input_path = display_path(a.input_path, options.full_path);
            b.input_path = display_path(b.input_path, options.full_path);
            const auto comparison = humtrace::compare_reports(a, b, options.compare);
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
            if (!options.json_path.empty()) {
                humtrace::write_file_atomically(options.json_path, humtrace::serialize_comparison_json(comparison));
                std::cout << "JSON comparison saved: " << options.json_path << '\n';
            }
            return 0;
        }
        Options options;
        if (args.empty() || args[0].rfind("--", 0) == 0 || !parse_options(args, 1, false, options)) return usage();
        if (!options.json_path.empty()) check_output(options.json_path, {args[0]}, options.force);
        auto report = run(args[0]);
        report.input_path = display_path(report.input_path, options.full_path);
        print_summary(report, options.quiet);
        if (!options.json_path.empty()) {
            humtrace::write_file_atomically(options.json_path, humtrace::serialize_report_json(report));
            std::cout << "JSON report saved: " << options.json_path << '\n';
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
