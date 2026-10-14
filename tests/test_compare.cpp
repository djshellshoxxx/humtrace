#include "humtrace/compare.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool c, const char* m) { if (!c) { std::cerr << "FAIL: " << m << '\n'; std::exit(1); } }

humtrace::AnalysisReport report(std::uint32_t rate, std::vector<std::vector<double>> freqs_per_channel) {
    humtrace::AnalysisReport r;
    r.sample_rate_hz = rate;
    r.channels = static_cast<std::uint16_t>(freqs_per_channel.size());
    r.frame_size_samples = 4096;
    r.duration_seconds = 10.0;
    for (std::size_t c = 0; c < freqs_per_channel.size(); ++c) {
        humtrace::ReportChannel ch;
        ch.channel_number = static_cast<std::uint16_t>(c + 1);
        std::size_t id = 1;
        for (const double f : freqs_per_channel[c]) {
            humtrace::Track t;
            t.id = id++;
            t.median_frequency_hz = f;
            t.median_level_dbfs = -20.0;
            ch.tracks.push_back(t);
        }
        r.channel_results.push_back(ch);
    }
    return r;
}
} // namespace

int main() {
    {
        const auto result = humtrace::compare_reports(report(48000, {{50.0, 150.0}}), report(48000, {{50.4, 300.0}}));
        require(result.matches.size() == 1 && std::abs(result.matches[0].delta_frequency_hz - 0.4) < 1e-9, "matching tone found");
        require(result.unmatched_a.size() == 1 && result.unmatched_a[0].median_frequency_hz == 150.0, "A-only track is unmatched");
        require(result.unmatched_b.size() == 1 && result.unmatched_b[0].median_frequency_hz == 300.0, "B-only track is unmatched");
        require(result.warnings.empty(), "compatible inputs carry no warnings");
        const auto json = humtrace::serialize_comparison_json(result);
        require(json.find("\"applied\": false") != std::string::npos, "no alignment/resampling recorded");
    }
    { // channels are not crossed and unmapped channels stay visible
        const auto result = humtrace::compare_reports(report(48000, {{50.0}, {60.0}}), report(44100, {{60.0}}));
        require(result.matches.empty(), "channel 1 (50 Hz) does not match B channel 1 (60 Hz)");
        require(result.unmatched_a.size() == 2 && result.unmatched_b.size() == 1, "all tracks accounted for");
        bool rate_warning = false;
        for (const auto& w : result.warnings) rate_warning = rate_warning || w.find("sample rates differ") != std::string::npos;
        require(rate_warning, "differing sample rates are flagged, not resampled");
    }
    { // explicit channel map
        humtrace::CompareSettings s;
        s.channel_map = {{2, 1}};
        const auto result = humtrace::compare_reports(report(48000, {{50.0}, {60.0}}), report(48000, {{60.2}}), s);
        require(result.matches.size() == 1 && result.matches[0].a.channel == 2, "explicit map pairs A2 with B1");
        require(result.unmatched_a.size() == 1 && result.unmatched_a[0].channel == 1, "unmapped A channel remains unmatched");
    }
    { // invalid input
        bool threw = false;
        humtrace::CompareSettings s;
        s.channel_map = {{9, 1}};
        try { (void)humtrace::compare_reports(report(48000, {{50.0}}), report(48000, {{50.0}}), s); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "unknown mapped channel rejected");
        threw = false;
        s = {};
        s.frequency_tolerance_hz = -1.0;
        try { (void)humtrace::compare_reports(report(48000, {{50.0}}), report(48000, {{50.0}}), s); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "negative tolerance rejected");
        threw = false;
        s = {};
        s.channel_map = {{1, 1}, {1, 1}};
        try { (void)humtrace::compare_reports(report(48000, {{50.0}}), report(48000, {{50.0}}), s); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "duplicate channel pairs rejected");
        threw = false;
        s.channel_map = {{1, 1}, {2, 1}};
        try { (void)humtrace::compare_reports(report(48000, {{50.0}, {60.0}}), report(48000, {{50.0}}), s); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "reusing a B channel rejected");
    }
    { // 65535 channels: the identity map must not wrap or take quadratic time
        humtrace::AnalysisReport wide;
        wide.sample_rate_hz = 8000;
        wide.channels = 65535;
        wide.frame_size_samples = 4096;
        wide.duration_seconds = 1.0;
        wide.channel_results.resize(65535);
        for (std::size_t i = 0; i < wide.channel_results.size(); ++i) wide.channel_results[i].channel_number = static_cast<std::uint16_t>(i + 1);
        const auto result = humtrace::compare_reports(wide, wide);
        require(result.settings.channel_map.size() == 65535 && result.matches.empty(), "wide recordings compare without wrapping");
    }
    { // tolerance boundary is inclusive; level delta is B minus A
        auto a = report(48000, {{50.0}});
        auto b = report(48000, {{51.0}});
        b.channel_results[0].tracks[0].median_level_dbfs = -14.0;
        humtrace::CompareSettings s;
        s.frequency_tolerance_hz = 1.0;
        const auto edge = humtrace::compare_reports(a, b, s);
        require(edge.matches.size() == 1, "distance equal to the tolerance matches");
        require(std::abs(edge.matches[0].delta_level_db - 6.0) < 1e-12 && std::abs(edge.matches[0].delta_frequency_hz - 1.0) < 1e-12, "deltas are B - A");
        s.frequency_tolerance_hz = 0.999;
        require(humtrace::compare_reports(a, b, s).matches.empty(), "just beyond the tolerance does not match");
    }
    std::cout << "All compare tests passed.\n";
}
