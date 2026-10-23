#include "humtrace/export.hpp"
#include "humtrace/music.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
class CommaNumbers final : public std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '.'; }
    std::string do_grouping() const override { return "\3"; }
};
} // namespace

int main() {
    const auto a4 = humtrace::nearest_note(440.0);
    require(a4.name == "A4" && std::abs(a4.cents) < 1e-9, "A4 maps to A4");
    const auto hum = humtrace::nearest_note(60.0);
    require(hum.name == "B1" && hum.cents < -45 && hum.cents > -50, "60 Hz is about B1 -49c");
    const auto fifty = humtrace::nearest_note(50.0);
    require(fifty.name == "G1" && std::abs(fifty.cents - 35.0) < 0.5, "50 Hz is G1 +35c");
    require(humtrace::nearest_note(261.6256).name == "C4", "middle C");
    require(humtrace::nearest_note(432.0, 432.0).name == "A4", "alternate A4 reference");
    require(humtrace::format_note(a4) == "A4 +0c", "format");
    const auto tiny = humtrace::nearest_note(1e-200, 1e200);
    require(tiny.nearest_hz > 0.0 && std::isfinite(tiny.nearest_hz) &&
            std::isfinite(tiny.cents) && std::abs(tiny.cents) <= 50.000001,
            "extreme finite frequency/reference ratios produce finite pitch labels");
    const auto huge = humtrace::nearest_note(1e200, 1e-200);
    require(huge.nearest_hz > 0.0 && std::isfinite(huge.nearest_hz) &&
            std::isfinite(huge.cents) && std::abs(huge.cents) <= 50.000001,
            "overflowing frequency/reference ratios produce finite pitch labels");
    const auto max_reference = humtrace::nearest_note(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    require(max_reference.nearest_hz == std::numeric_limits<double>::max() && max_reference.cents == 0.0,
            "equal extreme finite frequency and reference retain the reference");
    for (double bad : {0.0, -5.0, HUGE_VAL, std::nan("")}) {
        bool threw = false;
        try { (void)humtrace::nearest_note(bad); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "non-positive/non-finite frequency rejected");
    }

    humtrace::AnalysisReport report;
    report.sample_rate_hz = 1000;
    report.frame_size_samples = 500;
    humtrace::ReportChannel channel;
    channel.channel_number = 1;
    humtrace::Track track;
    track.id = 7;
    track.observations = {{0, 0, 60.0, -20.0}, {1, 250, 60.0, -20.0}};
    track.median_frequency_hz = 60.0;
    track.median_level_dbfs = -20.0;
    track.observed_frames = 2;
    channel.tracks.push_back(track);
    report.channel_results.push_back(channel);
    const auto csv = humtrace::serialize_tracks_csv(report);
    require(csv.rfind("channel,track_id,", 0) == 0, "csv header");
    require(csv.find("\n1,7,0,0.75,60,B1,") != std::string::npos, "csv row has times and note");
    const auto previous_locale = std::locale::global(std::locale(std::locale::classic(), new CommaNumbers));
    const auto localized_csv = humtrace::serialize_tracks_csv(report);
    std::locale::global(previous_locale);
    require(localized_csv == csv, "CSV uses decimal points independently of the application locale");
    report.channel_results[0].tracks[0].median_frequency_hz = std::numeric_limits<double>::quiet_NaN();
    report.channel_results[0].tracks[0].drift_hz_per_second = std::numeric_limits<double>::infinity();
    const auto missing_csv = humtrace::serialize_tracks_csv(report);
    require(missing_csv.find("nan") == std::string::npos && missing_csv.find("inf") == std::string::npos &&
            missing_csv.find("\n1,7,0,0.75,,,,-20,,2,0\n") != std::string::npos,
            "CSV leaves unavailable measurements empty rather than printing NaN or infinity");
    report.sample_rate_hz = 0;
    bool invalid_rate = false;
    try { (void)humtrace::serialize_tracks_csv(report); } catch (const std::invalid_argument&) { invalid_rate = true; }
    require(invalid_rate, "CSV refuses an unknown sample rate rather than emitting invalid times");
    std::cout << "All music tests passed.\n";
}
