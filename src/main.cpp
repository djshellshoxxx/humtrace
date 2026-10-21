#include "humtrace/compare.hpp"
#include "humtrace/export.hpp"
#include "humtrace/music.hpp"
#include "humtrace/session.hpp"
#include "humtrace/wav.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif
#include <atomic>
#include <csignal>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::atomic<bool> g_cancel{false};
double g_a4_hz = 440.0; // note-name tuning reference (--a4)
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
                      << track.median_frequency_hz << " Hz ("
                      << (track.median_frequency_hz > 0.0
                              ? humtrace::format_note(humtrace::nearest_note(track.median_frequency_hz, g_a4_hz))
                              : std::string("-"))
                      << "), drift " << track.drift_hz_per_second
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
    std::cerr << "Usage: humtrace <input.wav> [--json <report.json>] [--csv <tracks.csv>] [--a4 <Hz>] [--quiet] [--full-path] [--force]\n"
                 "       humtrace compare <a.wav> <b.wav> [--tolerance <Hz>] [--map <a:b[,a:b...]>]\n"
                 "                        [--json <comparison.json>] [--full-path] [--force]\n"
                 "  --csv        write one CSV row per track (with note name and cents)\n"
                 "  --a4 <Hz>    tuning reference for note names, 400-480 (default 440)\n"
                 "  --quiet      omit the per-frame lines\n"
                 "  --full-path  store input paths as typed (default: file name only)\n"
                 "  --force      replace an existing output file (default: refuse)\n";
    return 2;
}

struct Options {
    std::string json_path;
    std::string csv_path;
    double a4_hz{440.0};
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
        else if (a == "--csv") out.csv_path = value();
        else if (a == "--a4") {
            const auto& v = value();
            std::size_t used = 0;
            try { out.a4_hz = std::stod(v, &used); } catch (const std::exception&) { used = 0; }
            if (used != v.size() || !(out.a4_hz >= 400.0 && out.a4_hz <= 480.0))
                throw std::invalid_argument("--a4 needs a number between 400 and 480");
        }
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
    const auto parent = humtrace::path_from_utf8(output).parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent))
        throw std::runtime_error("output directory does not exist");
    if (!force && std::filesystem::exists(humtrace::path_from_utf8(output)))
        throw std::runtime_error("output file already exists; use --force to replace it");
}

std::string display_path(const std::string& path, bool full) {
    if (full) return path;
    const auto name = humtrace::path_from_utf8(path).filename().u8string(); // UTF-8 on every platform
    return std::string(name.begin(), name.end());
}

// Number of channels in a WAV header (cheap; used to validate options before a long analysis).
std::uint16_t channel_count(const std::string& path) {
    std::ifstream in(humtrace::path_from_utf8(path), std::ios::binary);
    if (!in) throw std::runtime_error("could not open input file");
    humtrace::WavStreamReader reader(in);
    return reader.metadata().channels;
}

#ifdef _WIN32
// The C runtime hands narrow argv in the ANSI code page; recover exact UTF-8 arguments instead.
std::vector<std::string> utf8_arguments() {
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::string> out;
    for (int i = 1; wide && i < count; ++i) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string text(size > 1 ? static_cast<std::size_t>(size - 1) : 0, '\0');
        if (size > 1) WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, text.data(), size, nullptr, nullptr);
        out.push_back(std::move(text));
    }
    if (wide) LocalFree(wide);
    return out;
}
#endif
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_interrupt);
#ifdef _WIN32
    (void)argc; (void)argv;
    const std::vector<std::string> args = utf8_arguments();
#else
    const std::vector<std::string> args(argv + 1, argv + argc);
#endif
    try {
        if (!args.empty() && args[0] == "compare") {
            Options options;
            if (args.size() < 3 || !parse_options(args, 3, true, options)) return usage();
            if (!options.json_path.empty()) check_output(options.json_path, {args[1], args[2]}, options.force);
            // Reject bad options before spending minutes on analysis.
            if (!std::isfinite(options.compare.frequency_tolerance_hz) || options.compare.frequency_tolerance_hz < 0.0)
                throw std::invalid_argument("frequency tolerance must be finite and non-negative");
            if (!options.compare.channel_map.empty()) {
                const auto channels_a = channel_count(args[1]);
                const auto channels_b = channel_count(args[2]);
                std::set<std::uint16_t> seen_a, seen_b;
                for (const auto& [ca, cb] : options.compare.channel_map) {
                    if (ca > channels_a || cb > channels_b)
                        throw std::invalid_argument("--map refers to a channel the recording does not have");
                    if (!seen_a.insert(ca).second || !seen_b.insert(cb).second)
                        throw std::invalid_argument("channel map must pair each channel at most once");
                }
            }
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
        if (!options.csv_path.empty()) check_output(options.csv_path, {args[0]}, options.force);
        g_a4_hz = options.a4_hz;
        auto report = run(args[0]);
        report.input_path = display_path(report.input_path, options.full_path);
        print_summary(report, options.quiet);
        if (!options.json_path.empty()) {
            humtrace::write_file_atomically(options.json_path, humtrace::serialize_report_json(report));
            std::cout << "JSON report saved: " << options.json_path << '\n';
        }
        if (!options.csv_path.empty()) {
            humtrace::write_file_atomically(options.csv_path, humtrace::serialize_tracks_csv(report, options.a4_hz));
            std::cout << "CSV track list saved: " << options.csv_path << '\n';
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
