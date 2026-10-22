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
        const auto& levels = report.channel_results[0].frames[0].levels;
        require(levels.has_value() && levels->sample_count == 4096, "raw sample levels attached to every frame");
        require(std::abs(levels->rms_dbfs + 10.0) < 0.01, "two orthogonal tones have expected RMS power");
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
    { // presentation allocation must not change any analytical measurement
        const auto bytes = two_tone_wav(4096, 4096 * 3);
        std::istringstream plain_input(bytes, std::ios::binary), view_input(bytes, std::ios::binary);
        humtrace::WavStreamReader plain_reader(plain_input), view_reader(view_input);
        const auto plain = humtrace::analyze_wav(plain_reader, {});
        std::vector<humtrace::ChannelView> views;
        const auto visual = humtrace::analyze_wav(view_reader, {}, {}, {}, &views);
        require(!views.empty() && !views[0].spectrogram_dbfs.empty() && !views[0].waveform_min.empty(),
                "requested presentation data is generated");
        require(humtrace::serialize_report_json(plain) == humtrace::serialize_report_json(visual),
                "CLI and desktop analyses have identical measurements");
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
    { // validate_settings rejects each invalid class directly (not only via the fuzzer)
        auto rejects = [&](auto mutate) {
            humtrace::PipelineSettings s;
            mutate(s);
            try { humtrace::validate_settings(s); } catch (const std::invalid_argument&) { return true; }
            return false;
        };
        require(rejects([](auto& s) { s.support_threshold_dbfs = NAN; }), "NaN threshold");
        require(rejects([](auto& s) { s.minimum_prominence_db = -1.0; }), "negative prominence");
        require(rejects([](auto& s) { s.tracker.relative_tolerance = -0.1; }), "negative relative tolerance");
        require(rejects([](auto& s) { s.tracker.min_observed_frames = 0; }), "zero min frames");
        require(rejects([](auto& s) { s.spectrogram_rows = 0; }), "zero rows");
        require(rejects([](auto& s) { s.spectrogram_rows = std::size_t{1} << 62; }), "huge rows cannot wrap the memory estimate");
        require(rejects([](auto& s) { s.max_peaks_per_frame = 100000; }), "huge peak count");
        require(rejects([](auto& s) { s.spectrogram_min_hz = 0.0; }), "zero min frequency");
        humtrace::validate_settings({}); // defaults are valid
    }
    { // suppressed tracks are counted and reported
        humtrace::PipelineSettings s;
        s.tracker.min_observed_frames = 100; // nothing can reach this
        std::istringstream in(two_tone_wav(4096, 4096 * 3), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        const auto report = humtrace::analyze_wav(reader, s);
        require(report.channel_results[0].tracks.empty() && report.channel_results[0].suppressed_tracks == 2, "short tracks are counted, not silently lost");
        require(!report.warnings.empty() && report.warnings.back().find("not listed") != std::string::npos, "suppression produces a warning");
    }
    { // Each decoded sample must enter Welch exactly once, including the partial final hop.
        std::istringstream in(two_tone_wav(4096, 10000), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        const auto report = humtrace::analyze_wav(reader, {});
        const auto& psd = report.channel_results[0].psd;
        require(psd.has_value(), "pipeline produces a Welch summary");
        require(psd->samples_processed == 10000 && psd->segments_averaged == 3,
                "Welch counts unique samples and complete segments only");
        require(psd->mains_50.prominent_harmonics > 0, "Welch detects the synthetic 50 Hz tone");
    }
    std::cout << "All pipeline tests passed.\n";
}
