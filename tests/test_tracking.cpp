// Purpose: tests for tonal peak picking, persistent tone tracking, display
// builders, shared-tone comparison and their integration in analyze_wav_file.
#include "humtrace/analysis.hpp"
#include "humtrace/compare.hpp"
#include "humtrace/display.hpp"
#include "humtrace/spectrum.hpp"
#include "humtrace/tracking.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace humtrace;

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

template <class F> void require_throws(F f, const char* message) {
    try { f(); } catch (const std::exception&) { return; }
    require(false, message);
}

std::vector<double> tones(const std::vector<std::pair<double, double>>& hz_amp, double rate,
                          std::size_t n, double noise = 0.0, unsigned seed = 1) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> gauss(0.0, noise > 0.0 ? noise : 1.0);
    std::vector<double> s(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (const auto& [hz, amp] : hz_amp)
            s[i] += amp * std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / rate);
        if (noise > 0.0) s[i] += gauss(rng);
    }
    return s;
}

void put16(std::string& s, std::uint16_t v) { s.push_back(char(v & 255)); s.push_back(char(v >> 8)); }
void put32(std::string& s, std::uint32_t v) { put16(s, v & 0xffff); put16(s, std::uint16_t(v >> 16)); }

void write_wav(const fs::path& path, const std::vector<double>& samples, unsigned rate) {
    std::string data;
    for (const double v : samples)
        put16(data, std::uint16_t(std::int16_t(std::lround(std::clamp(v, -1.0, 1.0) * 32767))));
    std::string w = "RIFF";
    put32(w, 36 + std::uint32_t(data.size()));
    w += "WAVEfmt ";
    put32(w, 16); put16(w, 1); put16(w, 1); put32(w, rate); put32(w, rate * 2); put16(w, 2); put16(w, 16);
    w += "data";
    put32(w, std::uint32_t(data.size()));
    w += data;
    std::ofstream(path, std::ios::binary) << w;
}

void tonal_peaks_finds_tones_strongest_first() {
    const double rate = 8192;
    const auto spectrum = analyze_spectrum(tones({{440.0, 0.5}, {1000.0, 0.1}}, rate, 8192, 1e-4), rate);
    const auto peaks = find_tonal_peaks(spectrum, -60.0, 10.0, 8);
    require(peaks.size() >= 2, "two tones found");
    require(std::abs(peaks[0].frequency_hz - 440.0) < 0.2, "strongest is 440 Hz");
    require(std::abs(peaks[0].level_dbfs - 20 * std::log10(0.5)) < 0.5, "440 Hz level within 0.5 dB");
    require(std::abs(peaks[1].frequency_hz - 1000.0) < 0.2, "second is 1 kHz");
    require(peaks[0].prominence_db > 40.0, "tone prominence is large");
    require(find_tonal_peaks(spectrum, -60.0, 10.0, 1).size() == 1, "max_peaks honoured");
    require(find_tonal_peaks(spectrum, -3.0, 10.0, 8).empty(), "threshold excludes all");
    require_throws([&] { (void)find_tonal_peaks(spectrum, -60.0, -1.0, 8); }, "negative prominence rejected");
    require_throws([&] { (void)find_tonal_peaks(spectrum, std::nan(""), 10.0, 8); }, "NaN threshold rejected");
    const std::vector<SpectrumBin> silent(4097, SpectrumBin{0.0, -std::numeric_limits<double>::infinity()});
    require(find_tonal_peaks(silent, -60.0, 10.0, 8).empty(), "silence has no peaks");
}

std::vector<TonalPeak> one(double hz, double db = -20.0) { return {{hz, db, -100.0, 80.0}}; }

void tracker_steady_tone_and_gaps() {
    TrackerSettings s;
    s.min_duration_seconds = 0.0;
    s.min_frames = 3;
    ToneTracker t(s, 1000.0, 1000, 500);  // 1 Hz bins, 0.5 s hop, tolerance 1.5 Hz
    require(std::abs(t.tolerance_hz() - 1.5) < 1e-12, "default tolerance is 1.5 bins");
    for (std::size_t f = 0; f < 10; ++f)
        t.push(f, f == 4 || f == 5 ? std::vector<TonalPeak>{} : one(100.0 + 0.1 * static_cast<double>(f)));
    // Gap of 3 frames (> max_gap 2) splits the second tone.
    for (std::size_t f = 10; f < 20; ++f)
        t.push(f, (f >= 13 && f <= 15) ? std::vector<TonalPeak>{} : one(300.0));
    const auto tracks = t.finish();
    require(tracks.size() == 3, "gap<=2 joins, gap 3 splits (100 Hz, 300 Hz x2)");
    const auto& a = tracks[0];
    require(a.frames_detected == 8 && a.frames_spanned == 10, "detected/spanned counts");
    require(std::abs(a.persistence - 0.8) < 1e-12, "persistence");
    require(a.start_sample == 0 && a.end_sample == 9 * 500 + 1000, "sample span");
    require(std::abs(a.drift_hz_per_second - 0.2) < 1e-9, "drift 0.1 Hz/frame = 0.2 Hz/s");
    require(a.min_frequency_hz == 100.0 && std::abs(a.max_frequency_hz - 100.9) < 1e-9, "range");
    require(a.mains.nominal_hz == 50.0 && a.mains.harmonic == 2, "100 Hz near 2x50");
    require(tracks[1].median_frequency_hz == 300.0 && tracks[1].frames_detected == 3, "first 300 Hz track");
    require(tracks[2].start_sample == 16 * 500, "second 300 Hz track start");
    require(!t.truncated(), "not truncated");
}

void tracker_follows_slow_drift_and_rejects_short() {
    TrackerSettings s;
    s.min_duration_seconds = 0.0;
    s.min_frames = 5;
    ToneTracker t(s, 1000.0, 1000, 500);
    for (std::size_t f = 0; f < 40; ++f) {
        std::vector<TonalPeak> peaks = one(200.0 + 1.0 * static_cast<double>(f));  // 1 Hz/frame < tolerance
        if (f < 4) peaks.push_back({700.0, -30.0, -100.0, 70.0});                  // 4 frames only
        t.push(f, peaks);
    }
    const auto tracks = t.finish();
    require(tracks.size() == 1, "drifting tone is one track, short tone discarded");
    require(tracks[0].frames_detected == 40, "all drift frames associated");
    require(std::abs(tracks[0].drift_hz_per_second - 2.0) < 1e-9, "drift 2 Hz/s");
}

void tracker_min_duration_limits_and_errors() {
    TrackerSettings s;  // min 0.5 s at 0.1 s hop -> 5 frames
    ToneTracker t(s, 1000.0, 200, 100);
    require(t.effective_min_frames() == 5, "duration sets effective minimum frames");
    for (std::size_t f = 0; f < 4; ++f) t.push(f, one(100.0));
    require(t.finish().empty(), "track shorter than min duration discarded");

    TrackerSettings lim;
    lim.min_duration_seconds = 0.0;
    lim.max_active_tracks = 2;
    lim.max_tracks = 1;
    ToneTracker l(lim, 1000.0, 1000, 500);
    for (std::size_t f = 0; f < 5; ++f)
        l.push(f, {{100.0, -20, -100, 80}, {200.0, -21, -100, 79}, {300.0, -22, -100, 78}});
    const auto out = l.finish();
    require(l.truncated() && out.size() == 1, "active and output limits truncate and flag");

    ToneTracker o(lim, 1000.0, 1000, 500);
    o.push(3, {});
    require_throws([&] { o.push(3, {}); }, "non-increasing frame rejected");
    require_throws([] { ToneTracker bad(TrackerSettings{}, 0.0, 1000, 500); }, "zero rate rejected");
    TrackerSettings neg;
    neg.min_duration_seconds = -1.0;
    require_throws([&] { ToneTracker bad(neg, 1000.0, 1000, 500); }, "negative duration rejected");
}

void nearest_mains_bounds() {
    require(nearest_mains(180.4, 1.0).nominal_hz == 60.0, "180.4 near 3x60");
    require(nearest_mains(150.0, 1.0).harmonic == 3, "150 near 3x50");
    require(nearest_mains(1000.0, 1.0).nominal_hz == 0.0, "1 kHz exceeds harmonic limit");
    require(nearest_mains(77.0, 1.0).nominal_hz == 0.0, "77 Hz not near mains");
    require(nearest_mains(std::nan(""), 1.0).nominal_hz == 0.0, "NaN safe");
    require(nearest_mains(20.0, 1.0).nominal_hz == 0.0, "below fundamental");
}

void waveform_builder_buckets() {
    WaveformBuilder w(10, 4);  // 3 samples per bucket -> 4 buckets
    for (int i = 0; i < 12; ++i) w.add(i == 4 ? -0.5 : (i == 7 ? 2.0 : 0.1));
    const auto e = w.take();
    require(e.samples_per_bucket == 3 && e.minimum.size() == 4, "bucket layout");
    require(e.minimum[1] == -0.5F && e.maximum[2] == 1.0F, "min and clamped max");
    require(e.minimum[3] == 0.1F && e.maximum[3] == 0.1F, "last partial bucket");
    WaveformBuilder empty(8, 8);
    const auto z = empty.take();
    require(z.minimum[0] == 0.0F && z.maximum[0] == 0.0F, "unfilled bucket is zero");
    require_throws([] { WaveformBuilder bad(0, 4); }, "zero samples rejected");
}

void spectrogram_builder_locates_tone() {
    const double rate = 8192;
    SpectrogramBuilder b(10, 4, 64, rate, 1024);
    const auto spectrum = analyze_spectrum(tones({{1000.0, 0.5}}, rate, 1024), rate);
    for (int i = 0; i < 10; ++i) b.add_frame(spectrum);
    const auto g = b.take();
    require(g.frames_per_column == 3 && g.columns() == 4 && g.rows == 64, "spectrogram layout");
    require(g.row_edges_hz.size() == 65 && std::abs(g.row_edges_hz.back() - rate / 2) < 1e-9, "edges reach Nyquist");
    std::size_t best = 0;
    for (std::size_t r = 1; r < g.rows; ++r)
        if (g.at(0, r) > g.at(0, best)) best = r;
    require(g.row_edges_hz[best] <= 1000.0 + 8.0 && g.row_edges_hz[best + 1] >= 1000.0 - 8.0, "tone row");
    require(std::abs(g.at(3, best) - 20 * std::log10(0.5)) < 1.0, "max-hold keeps tone level");
    require_throws([&] { SpectrogramBuilder c(10, 4, 64, rate, 1024); c.add_frame({}); }, "size mismatch rejected");
}

void analysis_tracks_and_display(const fs::path& dir) {
    const unsigned rate = 8000;
    auto s = tones({{60.0, 0.2}, {180.0, 0.1}}, rate, rate * 8, 1e-3);
    // 1 kHz present from 2 s to 5 s only.
    for (std::size_t i = 2 * rate; i < 5 * rate; ++i)
        s[i] += 0.1 * std::sin(2.0 * std::numbers::pi * 1000.0 * static_cast<double>(i) / rate);
    const auto path = dir / "tracks.wav";
    write_wav(path, s, rate);
    AnalysisOptions o;
    o.frame_size_samples = 2048;
    const auto r = analyze_wav_file(path.string(), o);
    const auto& tr = r.channel_results[0].tracks;
    bool f60 = false, f180 = false, f1k = false;
    for (const auto& t : tr) {
        if (std::abs(t.median_frequency_hz - 60.0) < 0.5 && t.persistence > 0.95) f60 = t.mains.nominal_hz == 60.0;
        if (std::abs(t.median_frequency_hz - 180.0) < 0.5) f180 = t.mains.harmonic == 3;
        if (std::abs(t.median_frequency_hz - 1000.0) < 0.5) {
            const double start = static_cast<double>(t.start_sample) / rate;
            const double end = static_cast<double>(t.end_sample) / rate;
            f1k = start > 1.4 && start < 2.1 && end > 4.9 && end < 5.6;
        }
    }
    require(f60 && f180 && f1k, "60/180 Hz persistent and 1 kHz interval tracked");
    require(tr.size() == 3, "no spurious tracks in low noise");
    require(r.track_tolerance_hz > 0.0 && !r.tracks_truncated, "tracking settings echoed");
    require(r.display.size() == 1, "display built");
    const auto& d = r.display[0];
    require(d.waveform.minimum.size() <= o.waveform_buckets && !d.waveform.minimum.empty(), "waveform bounded");
    require(d.spectrogram.columns() == r.channel_results[0].frames.size(), "one column per frame when few");
    float wmax = 0.0F;
    for (const float v : d.waveform.maximum) wmax = std::max(wmax, v);
    require(wmax > 0.25F && wmax < 0.45F, "waveform peak matches signal");

    o.build_display = false;
    require(analyze_wav_file(path.string(), o).display.empty(), "display can be disabled");
    o.build_display = true;
    o.spectrogram_rows = 0;
    require_throws([&] { (void)analyze_wav_file(path.string(), o); }, "invalid display size rejected");

    // False-positive negative: white noise only yields no tracks at default settings.
    const auto noise_path = dir / "noise.wav";
    for (const unsigned seed : {9U, 10U, 11U, 12U, 13U})
    for (const std::size_t frame : {256U, 1024U, 4096U}) {
        write_wav(noise_path, tones({}, rate, rate * 10, 0.1, seed), rate);
        AnalysisOptions n;
        n.frame_size_samples = frame;
        n.support_threshold_dbfs = -120.0;
        const auto nr = analyze_wav_file(noise_path.string(), n);
        for (const auto& t : nr.channel_results[0].tracks) std::cerr << frame << ": " << t.median_frequency_hz << " Hz " << t.frames_detected << "/" << t.frames_spanned << " prom " << t.median_prominence_db << "\n";
        require(nr.channel_results[0].tracks.empty(),
                "white noise yields no tone tracks");
    }

    // Shared tones across files: 60 Hz in both, 1 kHz only in the first.
    const auto other = dir / "other.wav";
    write_wav(other, tones({{60.0, 0.2}, {440.0, 0.1}}, rate, rate * 6, 1e-3), rate);
    AnalysisOptions c;
    c.frame_size_samples = 2048;
    c.build_display = false;
    const std::vector<FileSummary> files{summarize(analyze_wav_file(path.string(), c)),
                                         summarize(analyze_wav_file(other.string(), c))};
    const auto shared = find_shared_tones(files);
    require(shared.size() == 1 && std::abs(shared[0].frequency_hz - 60.0) < 0.5, "only 60 Hz shared");
    require(shared[0].files == std::vector<std::size_t>({0, 1}), "shared file indices");
    const auto json = serialize_comparison_json(files);
    require(json.find("\"comparison_schema_version\": \"1.0\"") != std::string::npos, "comparison schema");
    require(json.find("\"shared_tones\"") != std::string::npos, "shared tones in JSON");
    require(format_comparison(files).find("do not show a common source") != std::string::npos, "qualified text");
}
} // namespace

int main() {
    const auto dir = fs::temp_directory_path() / ("humtrace_tracking_" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    tonal_peaks_finds_tones_strongest_first();
    tracker_steady_tone_and_gaps();
    tracker_follows_slow_drift_and_rejects_short();
    tracker_min_duration_limits_and_errors();
    nearest_mains_bounds();
    waveform_builder_buckets();
    spectrogram_builder_locates_tone();
    analysis_tracks_and_display(dir);
    fs::remove_all(dir);
    std::cout << "tracking tests passed\n";
    return 0;
}
