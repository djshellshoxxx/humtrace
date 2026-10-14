// Randomized settings-matrix fuzzer. Each step re-randomizes EVERY setting (and the input
// recording) from a fresh entropy-derived seed, then exercises one fundamental feature
// (analyze, serialize, compare, cancel, export). Steps alternate features, so each feature
// is hit under many different complete settings combinations. Only errors are recorded.
//
// usage: settings_fuzz [seconds=30] [--replay <step-seed>]
#include "humtrace/compare.hpp"
#include "humtrace/session.hpp"
#include "humtrace/sha256.hpp"

#include <chrono>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace humtrace;
namespace fs = std::filesystem;

// Fresh seed for every step: OS entropy mixed with the clock and a counter.
std::uint64_t fresh_seed() {
    static std::uint64_t counter = 0;
    std::random_device rd;
    const auto now = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return (static_cast<std::uint64_t>(rd()) << 32) ^ rd() ^ (now * 0x9E3779B97F4A7C15ULL) ^ (++counter * 0xBF58476D1CE4E5B9ULL);
}

struct Rng {
    std::mt19937_64 gen;
    explicit Rng(std::uint64_t seed) : gen(seed) {}
    double real(double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(gen); }
    std::size_t integer(std::size_t lo, std::size_t hi) { return std::uniform_int_distribution<std::size_t>(lo, hi)(gen); }
    bool chance(double p) { return real(0, 1) < p; }
    template <class T> const T& pick(const std::vector<T>& v) { return v[integer(0, v.size() - 1)]; }
};

// A value that is usually in-domain but sometimes an edge or invalid extreme.
double level(Rng& r, double lo, double hi, bool& invalid) {
    if (r.chance(0.04)) {
        invalid = true;
        return r.pick<double>({std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()});
    }
    return r.chance(0.1) ? r.pick<double>({lo, hi}) : r.real(lo, hi);
}
double nonneg(Rng& r, double hi, bool& invalid) {
    if (r.chance(0.04)) {
        invalid = true;
        return r.pick<double>({-1.0, -1e-9, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()});
    }
    return r.chance(0.1) ? r.pick<double>({0.0, hi}) : r.real(0.0, hi);
}
std::size_t count(Rng& r, std::size_t lo, std::size_t hi, bool& invalid, bool zero_invalid = true) {
    if (zero_invalid && r.chance(0.03)) { invalid = true; return 0; }
    return r.chance(0.1) ? r.pick<std::size_t>({lo, hi}) : r.integer(lo, hi);
}

struct Step {
    std::uint64_t seed{};
    PipelineSettings settings;
    bool invalid{};          // oracle: settings contain a documented-invalid value
    // input recording
    std::uint32_t rate{};
    std::uint16_t channels{};
    int format{};            // 0 pcm8, 1 pcm16, 2 pcm24, 3 pcm32, 4 float32, 5 float64
    double seconds{};
    std::vector<double> tones;
    double noise{};
    std::string describe() const {
        std::ostringstream o;
        o << "seed=" << seed << " rate=" << rate << " ch=" << channels << " fmt=" << format << " sec=" << seconds
          << " tones=" << tones.size() << " noise=" << noise << " invalid=" << invalid
          << " thr=" << settings.support_threshold_dbfs << " prom=" << settings.minimum_prominence_db
          << " rows=" << settings.spectrogram_rows << " minhz=" << settings.spectrogram_min_hz << " buckets=" << settings.waveform_buckets
          << " peaks=" << settings.max_peaks_per_frame << " pklvl=" << settings.peak_min_level_dbfs << " pkprom=" << settings.peak_min_prominence_db
          << " tol=" << settings.tracker.fixed_hz_tolerance << " rel=" << settings.tracker.relative_tolerance
          << " binfrac=" << settings.tracker.bin_fraction << " gap=" << settings.tracker.max_gap_frames
          << " minobs=" << settings.tracker.min_observed_frames << " maxres=" << settings.max_result_bytes
          << " maxwin=" << settings.max_interleaved_window_samples;
        return o.str();
    }
};

// Randomize every setting and the input from one seed (so a step is exactly replayable).
Step randomize(std::uint64_t seed) {
    Rng r(seed);
    Step s;
    s.seed = seed;
    auto& p = s.settings;
    bool bad = false;
    p.support_threshold_dbfs = level(r, -140, 0, bad);
    p.minimum_prominence_db = nonneg(r, 60, bad);
    p.peak_min_level_dbfs = level(r, -140, 0, bad);
    p.peak_min_prominence_db = nonneg(r, 60, bad);
    p.max_peaks_per_frame = count(r, 1, 40, bad);
    p.waveform_buckets = count(r, 1, 5000, bad);
    p.spectrogram_rows = count(r, 1, 300, bad);
    p.spectrogram_min_hz = r.chance(0.04) ? (bad = true, r.pick<double>({0.0, -5.0, std::numeric_limits<double>::quiet_NaN()})) : r.real(0.5, 500.0);
    p.spectrogram_floor_dbfs = level(r, -200, -20, bad);
    p.tracker.fixed_hz_tolerance = nonneg(r, 20, bad);
    p.tracker.relative_tolerance = nonneg(r, 0.1, bad);
    p.tracker.bin_fraction = nonneg(r, 4, bad);
    p.tracker.max_gap_frames = count(r, 0, 12, bad, false);
    p.tracker.min_observed_frames = count(r, 1, 12, bad);
    p.max_result_bytes = r.chance(0.05) ? 1000 : (1ULL << 20) * r.integer(1, 1024); // tiny = legitimately too small
    p.max_interleaved_window_samples = r.chance(0.05) ? 16 : (1ULL << 24);
    s.invalid = bad;
    s.rate = r.pick<std::uint32_t>({1000, 4000, 8000, 11025, 22050, 44100, 48000, 96000});
    s.channels = static_cast<std::uint16_t>(r.integer(1, 3));
    s.format = static_cast<int>(r.integer(0, 5));
    s.seconds = r.real(0.5, 3.0);
    for (std::size_t i = 0, n = r.integer(0, 4); i < n; ++i)
        s.tones.push_back(r.real(5.0, s.rate * 0.45));
    s.noise = r.chance(0.5) ? 0.0 : std::pow(10.0, r.real(-80, -20) / 20.0);
    return s;
}

void put(std::string& b, std::uint64_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) b.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
}

std::string make_wav(const Step& s, Rng& r) {
    static constexpr int bits[] = {8, 16, 24, 32, 32, 64};
    const int depth = bits[s.format];
    const auto frames = static_cast<std::size_t>(s.seconds * s.rate);
    std::string data;
    std::normal_distribution<double> gauss(0.0, 1.0);
    for (std::size_t i = 0; i < frames; ++i)
        for (std::uint16_t c = 0; c < s.channels; ++c) {
            double v = s.noise * gauss(r.gen);
            for (std::size_t t = 0; t < s.tones.size(); ++t)
                v += 0.5 / static_cast<double>(s.tones.size()) * std::sin(2 * std::numbers::pi * s.tones[t] * (c + 1) * 0.5 * static_cast<double>(i) / s.rate);
            v = std::max(-1.0, std::min(0.999, v));
            if (s.format == 0) put(data, static_cast<std::uint8_t>(std::lround(v * 127) + 128), 1);
            else if (s.format <= 3) {
                const auto m = static_cast<double>((1LL << (depth - 1)) - 1);
                put(data, static_cast<std::uint64_t>(static_cast<std::int64_t>(std::lround(v * m))), depth / 8);
            } else if (s.format == 4) {
                const auto f = static_cast<float>(v);
                std::uint32_t u; std::memcpy(&u, &f, 4); put(data, u, 4);
            } else {
                std::uint64_t u; std::memcpy(&u, &v, 8); put(data, u, 8);
            }
        }
    std::string body = "fmt ";
    put(body, 16, 4); put(body, s.format >= 4 ? 3 : 1, 2); put(body, s.channels, 2); put(body, s.rate, 4);
    put(body, static_cast<std::uint64_t>(s.rate) * s.channels * (depth / 8), 4); put(body, static_cast<std::uint64_t>(s.channels) * (depth / 8), 2);
    put(body, static_cast<std::uint64_t>(depth), 2);
    body += "data"; put(body, data.size(), 4); body += data;
    if (data.size() & 1U) body.push_back('\0');
    std::string out = "RIFF";
    put(out, body.size() + 4, 4);
    return out + "WAVE" + body;
}

std::vector<std::string> g_errors;
std::size_t g_ok = 0, g_rejected = 0, g_legit_limit = 0, g_cancelled = 0; // coverage counters (not errors)
void record(const Step& s, const std::string& feature, const std::string& what) {
    g_errors.push_back("[" + feature + "] " + what + "\n    replay: settings_fuzz --replay " + std::to_string(s.seed) + "\n    " + s.describe());
}

bool json_sane(const std::string& j) {
    int depth = 0;
    bool in_string = false;
    for (std::size_t i = 0; i < j.size(); ++i) {
        const char c = j[i];
        if (in_string) { if (c == '\\') ++i; else if (c == '"') in_string = false; continue; }
        if (c == '"') in_string = true;
        else if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') { if (--depth < 0) return false; }
        else if ((c == 'n' || c == 'i' || c == 'N' || c == 'I') && j.compare(i, 3, "nan") == 0) return false;
        else if (c == 'i' && j.compare(i, 3, "inf") == 0) return false;
        else if (c == '-' && j.compare(i, 4, "-inf") == 0) return false;
    }
    return depth == 0 && !in_string;
}

void check_result(const Step& s, const std::string& path, const AnalysisResult& res, const std::string& feature) {
    const auto& rep = res.report;
    const auto bad = [&](const std::string& m) { record(s, feature, "invariant: " + m); };
    if (rep.channels != s.channels || rep.sample_rate_hz != s.rate) bad("metadata mismatch");
    const auto n = static_cast<std::size_t>(s.seconds * s.rate);
    std::size_t expected_frames = 0;
    if (rep.frame_size_samples <= n) {
        const auto full = (n - rep.frame_size_samples) / rep.hop_size_samples + 1;
        const auto covered = (full - 1) * rep.hop_size_samples + rep.frame_size_samples;
        expected_frames = full + (covered < n ? 1 : 0); // one end-aligned window covers the tail
    }
    if (rep.hop_size_samples * 2 != rep.frame_size_samples) bad("hop != window/2");
    if (rep.channel_results.size() != s.channels) bad("channel count");
    std::ifstream in(path, std::ios::binary);
    if (rep.input_sha256 != sha256_hex(in)) bad("sha256 does not match input bytes");
    if (rep.support_threshold_dbfs != s.settings.support_threshold_dbfs || rep.minimum_prominence_db != s.settings.minimum_prominence_db) bad("report does not echo harmonic settings");
    for (const auto& ch : rep.channel_results) {
        if (ch.frames.size() != expected_frames) bad("frame count " + std::to_string(ch.frames.size()) + " != " + std::to_string(expected_frames));
        for (const auto& t : ch.tracks) {
            if (t.observations.size() < s.settings.tracker.min_observed_frames) bad("track shorter than min_observed_frames");
            if (t.observed_frames != t.observations.size() || t.last_frame < t.first_frame) bad("track summary inconsistent");
            if (t.missing_frames != t.last_frame - t.first_frame + 1 - t.observed_frames) bad("missing frame arithmetic");
            if (t.missing_frames > 0 && s.settings.tracker.max_gap_frames == 0) bad("gap despite max_gap_frames=0");
            for (std::size_t i = 1; i < t.observations.size(); ++i) {
                if (t.observations[i].frame_index <= t.observations[i - 1].frame_index) bad("observations not increasing");
                else if (t.observations[i].frame_index - t.observations[i - 1].frame_index > s.settings.tracker.max_gap_frames + 1) bad("gap longer than allowed");
            }
            if (!std::isfinite(t.median_frequency_hz) || !std::isfinite(t.drift_hz_per_second)) bad("non-finite track summary");
            if (t.median_frequency_hz <= 0 || t.median_frequency_hz > s.rate / 2.0) bad("track frequency outside (0, Nyquist]");
        }
    }
    if (res.views.size() != s.channels) { bad("view count"); return; }
    for (const auto& v : res.views) {
        if (v.spectrogram_dbfs.size() != expected_frames) bad("spectrogram columns");
        for (const auto& col : v.spectrogram_dbfs)
            if (col.size() != v.row_frequency_hz.size() || col.size() != s.settings.spectrogram_rows) { bad("spectrogram rows"); break; }
        for (std::size_t i = 0; i < v.waveform_min.size(); ++i)
            if (v.waveform_min[i] > v.waveform_max[i] || v.waveform_min[i] < -1.0001F || v.waveform_max[i] > 1.0001F) { bad("waveform envelope range"); break; }
        if (v.waveform_min.size() > s.settings.waveform_buckets) bad("waveform bucket bound");
        for (std::size_t i = 1; i < v.row_frequency_hz.size(); ++i)
            if (!(v.row_frequency_hz[i] > v.row_frequency_hz[i - 1])) { bad("row frequencies not increasing"); break; }
    }
    const auto json = serialize_report_json(rep);
    if (!json_sane(json)) bad("report JSON malformed or contains nan/inf tokens");
}

AnalysisResult run_analysis(const Step& s, const std::string& path, const SessionCallbacks& cb = {}) {
    return analyze_file(path, s.settings, cb, true);
}

void feature_analyze(const Step& s, const std::string& path) {
    try {
        const auto res = run_analysis(s, path);
        if (s.invalid) { record(s, "analyze", "invalid settings were accepted"); return; }
        ++g_ok;
        check_result(s, path, res, "analyze");
        // Determinism: identical input + settings => identical report.
        const auto again = run_analysis(s, path);
        if (serialize_report_json(res.report) != serialize_report_json(again.report)) record(s, "analyze", "non-deterministic report");
    } catch (const AnalysisCancelled&) {
        record(s, "analyze", "spurious cancellation");
    } catch (const std::invalid_argument&) {
        ++g_rejected;
        if (!s.invalid) record(s, "analyze", "valid settings rejected as invalid_argument");
    } catch (const std::runtime_error& e) {
        const std::string m = e.what();
        const bool legit = m.find("too long for the configured result memory") != std::string::npos ||
                           m.find("too many channels") != std::string::npos || m.find("too short") != std::string::npos;
        if (legit) ++g_legit_limit;
        if (!legit && !s.invalid) record(s, "analyze", std::string("unexpected runtime_error: ") + m);
    } catch (const std::exception& e) {
        record(s, "analyze", std::string("unexpected exception: ") + e.what());
    }
}

void feature_compare_export(const Step& s, const std::string& path, const fs::path& dir) {
    if (s.invalid) return; // validity is covered by the analyze feature
    try {
        const auto a = run_analysis(s, path);
        const auto b = run_analysis(s, path);
        const auto cmp = compare_reports(a.report, b.report);
        std::size_t tracks = 0;
        for (const auto& c : a.report.channel_results) tracks += c.tracks.size();
        if (cmp.matches.size() != tracks || !cmp.unmatched_a.empty() || !cmp.unmatched_b.empty())
            record(s, "compare", "self-comparison did not match every track exactly once");
        for (const auto& m : cmp.matches)
            if (m.delta_frequency_hz != 0.0 || m.delta_level_db != 0.0) record(s, "compare", "self-comparison delta != 0");
        if (!json_sane(serialize_comparison_json(cmp))) record(s, "compare", "comparison JSON malformed");
        const auto out = (dir / "report.json").string();
        write_file_atomically(out, serialize_report_json(a.report));
        std::ifstream f(out, std::ios::binary);
        std::stringstream ss; ss << f.rdbuf();
        if (ss.str() != serialize_report_json(a.report)) record(s, "export", "exported bytes differ from serialized report");
        for (const auto& entry : fs::directory_iterator(dir))
            if (entry.path().string().find(".humtrace-tmp") != std::string::npos) record(s, "export", "temp file left behind");
    } catch (const std::runtime_error& e) {
        const std::string m = e.what();
        if (m.find("too long") == std::string::npos && m.find("too many channels") == std::string::npos && m.find("too short") == std::string::npos)
            record(s, "compare/export", std::string("unexpected error: ") + m);
    } catch (const std::exception& e) {
        record(s, "compare/export", std::string("unexpected exception: ") + e.what());
    }
}

void feature_cancel(const Step& s, const std::string& path, Rng& r) {
    if (s.invalid) return;
    const auto stop_after = r.integer(0, 6);
    std::size_t polls = 0;
    SessionCallbacks cb;
    cb.cancelled = [&] { return ++polls > stop_after; };
    try {
        (void)run_analysis(s, path, cb);
    } catch (const AnalysisCancelled&) {
        ++g_cancelled;
    } catch (const std::runtime_error& e) {
        const std::string m = e.what();
        if (m.find("too long") == std::string::npos && m.find("too many channels") == std::string::npos && m.find("too short") == std::string::npos)
            record(s, "cancel", std::string("unexpected error: ") + m);
    } catch (const std::exception& e) {
        record(s, "cancel", std::string("unexpected exception: ") + e.what());
    }
    // The source must be untouched and a later full run must still work.
    feature_analyze(s, path);
}

void run_step(std::uint64_t seed, int feature, const fs::path& dir) {
    const Step s = randomize(seed);
    Rng r(seed ^ 0xA5A5A5A5ULL);
    const auto path = (dir / "input.wav").string();
    { std::ofstream f(path, std::ios::binary | std::ios::trunc); const auto wav = make_wav(s, r); f.write(wav.data(), static_cast<std::streamsize>(wav.size())); }
    switch (feature) {
    case 0: feature_analyze(s, path); break;
    case 1: feature_compare_export(s, path, dir); break;
    default: feature_cancel(s, path, r); break;
    }
}
} // namespace

int main(int argc, char** argv) {
    double seconds = 30.0;
    std::uint64_t replay = 0;
    bool do_replay = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--replay" && i + 1 < argc) { replay = std::stoull(argv[++i]); do_replay = true; }
        else seconds = std::stod(a);
    }
    const auto dir = fs::temp_directory_path() / ("humtrace-fuzz-" + std::to_string(fresh_seed() % 1000000007ULL));
    fs::create_directories(dir);
    std::size_t steps = 0;
    if (do_replay) {
        for (int f = 0; f < 3; ++f) run_step(replay, f, dir);
        steps = 3;
    } else {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        int feature = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            run_step(fresh_seed(), feature, dir); // every step: all settings re-randomized from a fresh seed
            feature = (feature + 1) % 3;
            ++steps;
        }
    }
    fs::remove_all(dir);
    std::cout << "settings_fuzz: " << steps << " randomized steps, " << g_errors.size() << " error(s) [coverage: " << g_ok
              << " analyses verified, " << g_rejected << " invalid-settings rejections, " << g_legit_limit << " legitimate limit refusals, "
              << g_cancelled << " cancellations]\n";
    for (const auto& e : g_errors) std::cout << e << '\n';
    return g_errors.empty() ? 0 : 1;
}
