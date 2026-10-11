#include "humtrace/report.hpp"

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

    const auto json = humtrace::serialize_report_json(report);
    require(json.find("\"schema_version\": \"1.0\"") != std::string::npos,
            "report carries a schema version");
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
