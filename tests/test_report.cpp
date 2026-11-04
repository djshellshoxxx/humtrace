#include "humtrace/report.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void emits_versioned_report_with_escaped_path_and_measurements() {
    humtrace::AnalysisReport report;
    report.input_path = "field\"mic.wav";
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
    humtrace::ToneTrack track;
    track.start_sample = 0;
    track.end_sample = 96000;
    track.median_frequency_hz = 120.5;
    track.drift_hz_per_second = std::nan("");
    track.mains = {60.0, 2, 0.5};
    report.channel_results[0].tracks.push_back(track);
    report.channel_results[0].tracks.push_back(humtrace::ToneTrack{});

    const auto json = humtrace::serialize_report_json(report);
    require(json.find("\"schema_version\": \"1.2\"") != std::string::npos,
            "report carries a schema version");
    require(json.find("\"end_seconds\": 2") != std::string::npos, "track end time in seconds");
    require(json.find("\"drift_hz_per_second\": null") != std::string::npos, "non-finite drift is null");
    require(json.find("{\"nominal_hz\": 60, \"harmonic\": 2, \"deviation_hz\": 0.5}") != std::string::npos,
            "near-mains proximity serialized");
    require(json.find("\"near_mains_multiple\": null") != std::string::npos, "no proximity is null");
    require(json.find("\"tone_tracking\"") != std::string::npos, "tracking settings serialized");
    require(json.find("field\\\"mic.wav") != std::string::npos,
            "quotes in the input path are escaped");
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
}
} // namespace

int main() {
    emits_versioned_report_with_escaped_path_and_measurements();
    encodes_non_finite_measurements_as_json_null();
    std::cout << "All report tests passed.\n";
}
