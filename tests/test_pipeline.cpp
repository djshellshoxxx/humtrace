#include "humtrace/pipeline.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void u16(std::string& b, std::uint16_t v) { b.push_back(static_cast<char>(v & 0xff)); b.push_back(static_cast<char>(v >> 8)); }
void put_s16(std::string& b, int v) { u16(b, static_cast<std::uint16_t>(static_cast<std::int16_t>(v))); }
void u32(std::string& b, std::uint32_t v) { for (unsigned s = 0; s < 32; s += 8) b.push_back(static_cast<char>((v >> s) & 0xff)); }

// Mono 16-bit WAV containing 50 Hz and 120 Hz tones.
std::string two_tone_wav(std::uint32_t rate, std::uint32_t frames) {
    std::string data;
    for (std::uint32_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        const double v = 0.4 * std::sin(2.0 * std::numbers::pi * 50.0 * t) +
                         0.2 * std::sin(2.0 * std::numbers::pi * 120.0 * t);
        u16(data, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(v * 32767.0))));
    }
    std::string body = "fmt ";
    u32(body, 16); u16(body, 1); u16(body, 1); u32(body, rate); u32(body, rate * 2); u16(body, 2); u16(body, 16);
    body += "data"; u32(body, static_cast<std::uint32_t>(data.size())); body += data;
    std::string out = "RIFF";
    u32(out, static_cast<std::uint32_t>(body.size() + 4));
    out += "WAVE"; out += body;
    return out;
}
} // namespace

int main() {
    {
        std::istringstream in(two_tone_wav(4096, 4096 * 3), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        double last = 0.0;
        const auto report = humtrace::analyze_wav(reader, {}, [&](double f) { last = f; });
        require(last == 1.0, "progress reaches completion");
        require(report.channel_results.size() == 1 && report.channel_results[0].frames.size() == 5,
                "overlapping frames are produced");
        const auto& tracks = report.channel_results[0].tracks;
        require(tracks.size() == 2, "both tones form separate tracks");
        require(tracks[0].observed_frames == 5 && tracks[1].observed_frames == 5, "tracks persist across frames");
        require(std::abs(tracks[0].median_frequency_hz - 50.0) < 0.3 &&
                std::abs(tracks[1].median_frequency_hz - 120.0) < 0.3, "track frequencies are measured");
    }
    {
        std::istringstream in(two_tone_wav(4096, 4096 * 3), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        bool threw = false;
        try { (void)humtrace::analyze_wav(reader, {}, {}, [] { return true; }); }
        catch (const humtrace::AnalysisCancelled&) { threw = true; }
        require(threw, "cancellation aborts analysis");
    }
    {
        std::istringstream in(two_tone_wav(4096, 15), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        bool threw = false;
        try { (void)humtrace::analyze_wav(reader, {}); } catch (const std::runtime_error&) { threw = true; }
        require(threw, "too-short audio is rejected");
    }
    {
        std::istringstream in(two_tone_wav(4096, 4096 * 3), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        humtrace::PipelineSettings tight;
        tight.max_result_bytes = 1000;
        bool threw = false;
        try { (void)humtrace::analyze_wav(reader, tight); } catch (const std::runtime_error&) { threw = true; }
        require(threw, "result memory limit is enforced up front");
    }
    { // the final partial hop is analyzed (end-aligned window) and the waveform covers the whole file
        std::istringstream in(two_tone_wav(4096, 10000), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        double last = 0.0;
        std::vector<humtrace::ChannelView> views;
        const auto report = humtrace::analyze_wav(reader, {}, [&](double f) { last = f; }, {}, &views);
        const auto& frames = report.channel_results[0].frames;
        require(frames.size() == 4 && frames.back().start_sample == 10000 - 4096, "tail window ends at the last sample");
        require(last == 1.0, "progress completes");
        require(views[0].waveform_min.size() * views[0].waveform_samples_per_bucket >= 10000 &&
                (views[0].waveform_min.size() - 1) * views[0].waveform_samples_per_bucket < 10000,
                "envelope spans every sample, not just analyzed windows");
    }
    { // unusably low sample rates are a clean runtime error
        std::istringstream in(two_tone_wav(4, 64), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        bool threw = false;
        try { (void)humtrace::analyze_wav(reader, {}); } catch (const std::runtime_error& e) { threw = std::string(e.what()).find("sample rate") != std::string::npos; }
        require(threw, "very low sample rate rejected clearly");
    }
    { // clipping is counted and warned about; suppressed tracks are reported
        std::string data;
        for (int i = 0; i < 4096 * 3; ++i) put_s16(data, std::sin(2.0 * std::numbers::pi * 60.0 * i / 4096.0) >= 0 ? 32767 : -32768);
        std::string body = "fmt ";
        u32(body, 16); u16(body, 1); u16(body, 1); u32(body, 4096); u32(body, 8192); u16(body, 2); u16(body, 16);
        body += "data"; u32(body, static_cast<std::uint32_t>(data.size())); body += data;
        std::string wav = "RIFF";
        u32(wav, static_cast<std::uint32_t>(body.size() + 4));
        wav += "WAVE" + body;
        std::istringstream in(wav, std::ios::binary);
        humtrace::WavStreamReader reader(in);
        const auto report = humtrace::analyze_wav(reader, {});
        require(report.channel_results[0].clipped_samples > 10000, "full-scale samples counted");
        require(!report.warnings.empty() && report.warnings[0].find("full scale") != std::string::npos, "clipping warning emitted");
        require(report.max_peaks_per_frame == 8 && report.peak_min_prominence_db == 10.0, "pipeline settings recorded in the report");
    }
    std::cout << "All pipeline tests passed.\n";
}
