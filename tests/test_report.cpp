#include "humtrace/report.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void emits_versioned_report_with_escaped_path_and_measurements() {
    humtrace::AnalysisReport report;
    report.input_path = "field\"mic.wav";
    report.input_sha256 = "abc123";
    report.sample_rate_hz = 48000;
    report.channels = 1;
    report.bit_depth = 24;
    report.duration_seconds = 2.0;
    report.frame_size_samples = 32768;
    report.hop_size_samples = 16384;
    report.channel_results.resize(1);
    report.channel_results[0].channel_number = 1;
    report.channel_results[0].frames.push_back({
        0, {60.25, -18.0}, {50.0, 0, {}}, {60.0, 1, {{1, 60.0, 60.0, -18.0, -70.0, 52.0, true}}}});

    const auto json = humtrace::serialize_report_json(report);
    require(json.find("\"schema_version\": \"1.3\"") != std::string::npos,
            "report carries a schema version");
    require(json.find("field\\\"mic.wav") != std::string::npos,
            "quotes in the input path are escaped");
    require(json.find("\"sha256\": \"abc123\"") != std::string::npos, "input hash is serialized");
    require(json.find("\"tracks\": [") != std::string::npos, "tracks array is serialized");
    require(json.find("\"frequency_hz\": 60.25") != std::string::npos,
            "interpolated frequency is retained");
    require(json.find("\"prominence_db\": 52") != std::string::npos,
            "harmonic prominence is retained");
}

void encodes_non_finite_measurements_as_json_null() {
    humtrace::AnalysisReport report;
    report.sample_rate_hz = 48000;
    report.channels = 1;
    report.frame_size_samples = 4096;
    report.hop_size_samples = 2048;
    report.channel_results.resize(1);
    report.channel_results[0].frames.push_back({0, {0.0, -std::numeric_limits<double>::infinity()},
                                                 {50.0, 0, {}}, {60.0, 0, {}}});
    const auto json = humtrace::serialize_report_json(report);
    require(json.find("\"level_dbfs\": null") != std::string::npos,
            "negative infinity is represented with valid JSON null");
    require(json.find("\"strongest_peak\": {\"frequency_hz\": null") != std::string::npos &&
            json.find("\"raw_bin_frequency_hz\": null") != std::string::npos,
            "an absent strongest peak cannot appear to measure a zero-Hz component");
}
void invalid_utf8_paths_still_yield_valid_json() {
    humtrace::AnalysisReport report;
    report.input_path = std::string("bad\xff\xfe.wav \xc3\xa9 \xed\xa0\x80");
    report.sample_rate_hz = 8000;
    report.channels = 1;
    const auto json = humtrace::serialize_report_json(report);
    require(json.find("\xff") == std::string::npos && json.find("\xEF\xBF\xBD") != std::string::npos, "invalid bytes replaced with U+FFFD");
    require(json.find("\xc3\xa9") != std::string::npos, "valid UTF-8 preserved");
}
class CommaNumbers final : public std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '.'; }
    std::string do_grouping() const override { return "\3"; }
};
void json_numbers_are_independent_of_output_formatting() {
    humtrace::AnalysisReport report;
    report.sample_rate_hz = 48000;
    report.duration_seconds = 1.25;
    std::ostringstream out;
    const auto localized = std::locale(std::locale::classic(), new CommaNumbers);
    out.imbue(localized);
    out << std::hex << std::showpos << std::uppercase;
    humtrace::write_report_json(out, report);
    require(out.str().find("\"sample_rate_hz\": 48000") != std::string::npos &&
            out.str().find("\"duration_seconds\": 1.25") != std::string::npos,
            "JSON ignores caller locale and numeric formatting flags");
    require(out.getloc() == localized && (out.flags() & std::ios::basefield) == std::ios::hex &&
            (out.flags() & std::ios::showpos), "JSON restores caller stream formatting");
}
void ambiguity_links_are_explicit_and_channel_local() {
    humtrace::AnalysisReport report;
    report.sample_rate_hz = 1000;
    report.channels = 1;
    report.channel_results.resize(1);
    auto& channel = report.channel_results[0];
    channel.channel_number = 1;
    humtrace::Track track;
    track.id = 7;
    track.end_reason = humtrace::TrackEndReason::Ambiguous;
    track.end_ambiguity_id = 2;
    channel.tracks.push_back(track);
    channel.ambiguities.push_back({2, 3, 1500, humtrace::TrackAmbiguityKind::Split, {7}, {8, 9}});
    const auto json = humtrace::serialize_report_json(report);
    require(json.find("\"end_reason\": \"ambiguous\"") != std::string::npos &&
            json.find("\"start_ambiguity_id\": null, \"end_ambiguity_id\": 2") != std::string::npos,
            "track segments link to explicit ambiguity boundaries");
    require(json.find("\"ambiguities\": [{\"local_id\": 2, \"frame_index\": 3, \"start_sample\": 1500, \"kind\": \"split\", \"parent_track_ids\": [7], \"child_track_ids\": [8, 9]}") != std::string::npos,
            "channel-local ambiguity table preserves every parent and child ID");
    humtrace::ReportArrays arrays;
    arrays.frames = [](std::ostream&, std::size_t) {};
    arrays.tracks = [](std::ostream&, std::size_t) {};
    arrays.ambiguities = [](std::ostream& out, std::size_t) {
        humtrace::write_ambiguity_json(out, {2, 3, 1500, humtrace::TrackAmbiguityKind::Merge, {7, 8}, {9}});
    };
    std::ostringstream out;
    humtrace::write_report_json(out, report, &arrays);
    require(out.str().find("\"kind\": \"merge\", \"parent_track_ids\": [7, 8]") != std::string::npos,
            "disk report providers can supply ambiguity tables without materializing them");
}
void serializes_effective_settings_and_separates_resolution_from_uncertainty() {
    humtrace::AnalysisReport report;
    report.sample_rate_hz = 1000;
    report.channels = 1;
    report.spectrum_remove_mean = false;
    report.welch.remove_mean = true;
    report.peak_search = {20.0, 400.0, 3.0, 1, 6};
    report.harmonic_tolerance_hz = 2.5;
    report.channel_results.resize(1);
    humtrace::InterferenceFrame frame;
    frame.strongest_peak.frequency_hz = 60.25;
    frame.strongest_peak.raw_bin_frequency_hz = 60.0;
    frame.strongest_peak.bin_spacing_hz = 1.0;
    frame.strongest_peak.hann_main_lobe_width_hz = 4.0;
    frame.strongest_peak.width_hz = 1.5;
    frame.strongest_peak.interpolation_applied = true;
    report.channel_results[0].frames.push_back(frame);
    const auto json = humtrace::serialize_report_json(report);
    require(json.find("\"dc_removed\": true, \"partial_segments\": \"excluded\", \"guard_bins\": 1, \"radius_bins\": 6") != std::string::npos,
            "Welch report records effective DC and local-floor settings");
    require(json.find("\"dc_removed\": false") != std::string::npos &&
            json.find("\"harmonic_tolerance_hz\": 2.5") != std::string::npos &&
            json.find("\"minimum_frequency_hz\": 20, \"maximum_frequency_hz\": 400, \"minimum_separation_hz\": 3") != std::string::npos,
            "report records effective configured spectrum and peak search settings");
    require(json.find("\"raw_bin_frequency_hz\": 60, \"bin_spacing_hz\": 1, \"hann_main_lobe_width_hz\": 4") != std::string::npos &&
            json.find("\"width_hz\": 1.5, \"frequency_uncertainty_hz\": null") != std::string::npos,
            "raw resolution and peak width are not presented as validated estimator uncertainty");
    require(json.find("\"uncertainty_status\": \"not_characterized\"") != std::string::npos &&
            json.find("\"width_method\": \"raw-bin half-power crossings; linear interpolation in dB\"") != std::string::npos &&
            json.find("\"competing_peak_rule\": \"other local maximum within 4 bins and 10 dB; deterministic, uncalibrated\"") != std::string::npos,
            "report readers can distinguish uncharacterized uncertainty and uncalibrated peak heuristics");
}
} // namespace

int main() {
    invalid_utf8_paths_still_yield_valid_json();
    emits_versioned_report_with_escaped_path_and_measurements();
    encodes_non_finite_measurements_as_json_null();
    json_numbers_are_independent_of_output_formatting();
    ambiguity_links_are_explicit_and_channel_local();
    serializes_effective_settings_and_separates_resolution_from_uncertainty();
    std::cout << "All report tests passed.\n";
}
