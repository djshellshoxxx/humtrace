#include "findings.hpp"
#include "job_state.hpp"
#include "plot_math.hpp"

#include <cstdlib>
#include <iostream>

using namespace humtrace::ui;

static void require(bool c, const char* m) { if (!c) { std::cerr << "FAIL: " << m << '\n'; std::exit(1); } }

int main() {
    { // zoom/pan keep the range inside the recording and never invert it
        TimeRange v{0, 100};
        v = zoom(v, 50, 0.5, 100);
        require(std::abs(v.length() - 50) < 1e-9 && v.start >= 0 && v.end <= 100, "zoom halves the range");
        require(std::abs((50 - v.start) / v.length() - 0.5) < 1e-9, "zoom keeps the anchor fixed");
        v = pan(v, 1000, 100);
        require(v.end == 100 && std::abs(v.length() - 50) < 1e-9, "pan clamps at the end");
        v = zoom({0, 100}, 10, 1e-9, 100);
        require(v.length() >= kMinViewSeconds - 1e-12, "zoom has a minimum span");
        v = zoom({0, 100}, 10, 100.0, 100);
        require(v.start == 0 && v.end == 100, "zoom out clamps to the recording");
    }
    { // log axis round-trips
        const double p = log_position(1000, 20, 20000);
        require(std::abs(hz_from_log_position(p, 20, 20000) - 1000) < 1e-6, "log axis round trip");
        require(log_position(5, 20, 20000) == 0.0 && log_position(0, 20, 20000) == 0.0, "log axis clamps");
    }
    { // colormap is bounded and monotonic in luminance for the grayscale palette
        require(colormap(-1, Palette::Default).r == colormap(0, Palette::Default).r, "colormap clamps low");
        require(colormap(2, Palette::Grayscale).r == 255, "grayscale clamps high");
        require(colormap(0.3, Palette::Grayscale).r < colormap(0.7, Palette::Grayscale).r, "grayscale is monotonic");
        require(normalize_db(-INFINITY, -100, -10) == 0.0, "silence maps to the floor");
    }
    { // palette contrast (design tokens from docs/specs/gui.md)
        const Rgb canvas{0x11, 0x18, 0x20}, surface{0x19, 0x23, 0x2D};
        require(contrast_ratio({0xF0, 0xF3, 0xF6}, canvas) > 15.5, "primary text contrast on canvas");
        require(contrast_ratio({0xA6, 0xB3, 0xC2}, surface) > 7.0, "secondary text contrast on surface");
        require(contrast_ratio({0x59, 0xD8, 0xE8}, surface) > 6.0, "signal accent contrast on surface");
        require(contrast_ratio({0xFF, 0xBF, 0x69}, surface) > 6.0, "event accent contrast on surface");
        require(contrast_ratio({0xC4, 0xA7, 0xFF}, surface) > 6.0, "compare accent contrast on surface");
        require(contrast_ratio({0xFF, 0x77, 0x73}, surface) > 4.5, "error color contrast on surface");
    }
    { // state machine
        require(can_transition(JobState::Empty, JobState::Opening), "empty -> opening");
        require(!can_transition(JobState::Empty, JobState::Analyzing), "cannot analyze from empty");
        require(can_transition(JobState::Analyzing, JobState::Cancelling), "cancel while analyzing");
        require(can_transition(JobState::Cancelling, JobState::Cancelled), "cancelling -> cancelled");
        require(!can_transition(JobState::Complete, JobState::Analyzing), "no direct complete -> analyzing");
        require(can_transition(JobState::Complete, JobState::Invalidated), "settings change invalidates");
        require(is_active(JobState::Hashing) && !is_active(JobState::Complete), "activity classification");
    }
    { // findings: ordering and wording
        humtrace::AnalysisReport r;
        r.sample_rate_hz = 1000;
        r.channels = 2;
        r.frame_size_samples = 100;
        r.hop_size_samples = 50;
        r.channel_results.resize(2);
        r.channel_results[0].channel_number = 1;
        r.channel_results[1].channel_number = 2;
        auto track = [](std::size_t id, std::uint64_t first, std::uint64_t last, double hz) {
            humtrace::Track t;
            t.id = id;
            t.observations = {{0, first, hz, -20.0}, {1, last, hz, -20.0}};
            t.observed_frames = 2;
            t.missing_frames = 1;
            t.median_frequency_hz = hz;
            t.median_level_dbfs = -20.0;
            return t;
        };
        r.channel_results[1].tracks = {track(1, 0, 100, 50.0)};
        r.channel_results[0].tracks = {track(2, 200, 300, 60.0), track(1, 0, 100, 120.0)};
        const auto rows = build_findings(r);
        require(rows.size() == 3, "one row per track");
        require(rows[0].channel == 1 && rows[0].track_id == 1 && rows[2].channel == 2, "ordered by channel then start");
        require(std::abs(rows[0].end_seconds - 0.2) < 1e-9, "end includes the frame length");
        require(std::abs(rows[0].coverage() - 2.0 / 3.0) < 1e-9, "coverage = observed / spanned");
        const auto text = describe_finding(r, rows[0]);
        require(text.find("observed in 2 of 3 frames") != std::string::npos, "descriptive wording");
        require(text.find("does not identify a device") != std::string::npos, "limitations are stated");
        require(text.find("ground loop detected") == std::string::npos, "no source attribution");
    }
    {
        humtrace::AnalysisReport r;
        r.channel_results.resize(1);
        r.channel_results[0].channel_number = 1;
        humtrace::PsdSummary psd;
        psd.segments_averaged = 5;
        psd.broadband_median_dbfs2_per_hz = -90;
        r.channel_results[0].psd = psd;
        const auto text = summarize_report(r, {});
        require(text.find("Welch PSD") != std::string::npos && text.find("dBFS^2/Hz") != std::string::npos,
                "desktop summary exposes PSD with explicit density units");
    }
    std::cout << "All presentation tests passed.\n";
}
