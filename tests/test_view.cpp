#include "humtrace/pipeline.hpp"
#include "humtrace/view.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>

namespace {
void require(bool c, const char* m) { if (!c) { std::cerr << "FAIL: " << m << '\n'; std::exit(1); } }
void u16(std::string& b, std::uint16_t v) { b.push_back(static_cast<char>(v & 0xff)); b.push_back(static_cast<char>(v >> 8)); }
void u32(std::string& b, std::uint32_t v) { for (unsigned s = 0; s < 32; s += 8) b.push_back(static_cast<char>((v >> s) & 0xff)); }
std::string tone_wav(std::uint32_t rate, std::uint32_t frames, double hz, double amp) {
    std::string data;
    for (std::uint32_t i = 0; i < frames; ++i)
        u16(data, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(
            amp * 32767.0 * std::sin(2.0 * std::numbers::pi * hz * i / rate)))));
    std::string body = "fmt ";
    u32(body, 16); u16(body, 1); u16(body, 1); u32(body, rate); u32(body, rate * 2); u16(body, 2); u16(body, 16);
    body += "data"; u32(body, static_cast<std::uint32_t>(data.size())); body += data;
    std::string out = "RIFF";
    u32(out, static_cast<std::uint32_t>(body.size() + 4));
    return out + "WAVE" + body;
}
} // namespace

int main() {
    { // envelope
        humtrace::EnvelopeBuilder env(4, 8);
        const double s[8] = {0.1, -0.2, 0.5, 0.0, -0.9, 0.3, 0.2, 0.2};
        env.add(s, 3); env.add(s + 3, 5);
        std::vector<float> lo, hi;
        env.finish(lo, hi);
        require(lo.size() == 4 && hi.size() == 4, "one entry per filled bucket");
        require(lo[0] == -0.2F && hi[0] == 0.1F && lo[2] == -0.9F && hi[2] == 0.3F, "min/max per bucket");
        bool threw = false;
        try { humtrace::EnvelopeBuilder bad(0, 8); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "zero buckets rejected");
    }
    { // the partial last bucket is reported and holds its own min/max
        humtrace::EnvelopeBuilder env(4, 10); // 3 samples per bucket -> buckets of 3,3,3,1
        const double s[10] = {0.1, 0.2, 0.3, -0.1, -0.2, -0.3, 0.9, 0.8, 0.7, -0.5};
        env.add(s, 10);
        std::vector<float> lo, hi;
        env.finish(lo, hi);
        require(lo.size() == 4 && lo[3] == -0.5F && hi[3] == -0.5F, "partial final bucket is kept");
        require(env.samples_per_bucket() == 3, "bucket size is the ceiling of samples / buckets");
    }
    { // pipeline views
        std::istringstream in(tone_wav(4096, 4096 * 3, 60.0, 0.5), std::ios::binary);
        humtrace::WavStreamReader reader(in);
        std::vector<humtrace::ChannelView> views;
        const auto report = humtrace::analyze_wav(reader, {}, {}, {}, &views);
        require(views.size() == 1, "one view per channel");
        const auto& v = views[0];
        require(v.spectrogram_dbfs.size() == report.channel_results[0].frames.size(), "one spectrogram column per frame");
        require(v.spectrogram_dbfs[0].size() == v.row_frequency_hz.size(), "rows match edges");
        require(!v.waveform_max.empty() && v.waveform_max.size() <= 2048, "bounded waveform buckets");
        float peak = 0;
        for (const auto x : v.waveform_max) peak = std::max(peak, x);
        require(peak > 0.45F && peak < 0.55F, "waveform envelope reaches tone amplitude");
        // loudest spectrogram row should contain 60 Hz
        std::size_t best = 0;
        for (std::size_t r = 1; r < v.spectrogram_dbfs[0].size(); ++r)
            if (v.spectrogram_dbfs[0][r] > v.spectrogram_dbfs[0][best]) best = r;
        require(v.row_frequency_hz[best] <= 60.5 && (best + 1 >= v.row_frequency_hz.size() || v.row_frequency_hz[best + 1] > 59.0),
                "tone appears in the matching log-frequency row");
    }
    std::cout << "All view tests passed.\n";
}
