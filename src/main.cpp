#include "humtrace/pipeline.hpp"
#include "humtrace/report.hpp"
#include "humtrace/sha256.hpp"
#include "humtrace/wav.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void print_summary(const humtrace::AnalysisReport& report) {
    const auto rate = static_cast<double>(report.sample_rate_hz);
    const auto window = static_cast<double>(report.frame_size_samples);
    std::cout << std::fixed << std::setprecision(3)
              << "HumTrace analysis (" << window / rate << " s per frame)\n"
              << "Sample rate: " << report.sample_rate_hz << " Hz\n"
              << "Channels: " << report.channels << "\n"
              << "Bit depth: " << report.bit_depth << "\n"
              << "Duration: " << report.duration_seconds << " s\n"
              << "FFT bins: " << report.frame_size_samples / 2 + 1 << " ("
              << rate / window << " Hz/bin)\n"
              << "Timeline hop: " << static_cast<double>(report.hop_size_samples) / rate << " s\n"
              << "Input SHA-256: " << report.input_sha256 << '\n';
    for (const auto& channel : report.channel_results) {
        std::cout << "Channel " << channel.channel_number << " measured frames: "
                  << channel.frames.size() << '\n';
        for (const auto& frame : channel.frames) {
            const auto& peak = frame.strongest_peak;
            std::cout << "  " << static_cast<double>(frame.start_sample) / rate << " s: ";
            if (peak.frequency_hz == 0.0)
                std::cout << "no non-DC energy";
            else
                std::cout << "strongest non-DC component " << peak.frequency_hz << " Hz, "
                          << peak.level_dbfs << " dBFS";
            std::cout << "; 50 Hz bins over " << report.support_threshold_dbfs << " dBFS and "
                      << report.minimum_prominence_db << " dB local prominence: "
                      << frame.mains_50.supporting_harmonics << '/' << frame.mains_50.harmonics.size()
                      << "; 60 Hz: " << frame.mains_60.supporting_harmonics << '/'
                      << frame.mains_60.harmonics.size() << '\n';
        }
        for (const auto& track : channel.tracks)
            std::cout << "  Track " << track.id << ": observed in " << track.observed_frames << " of "
                      << track.observed_frames + track.missing_frames << " frames, median "
                      << track.median_frequency_hz << " Hz, drift " << track.drift_hz_per_second
                      << " Hz/s\n";
    }
    std::cout << "Interpretation: spectral components are measurements, not source attribution.\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && (argc != 4 || std::string(argv[2]) != "--json")) {
        std::cerr << "Usage: humtrace <input.wav> [--json <report.json>]\n";
        return 2;
    }
    try {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input)
            throw std::runtime_error("could not open input file");
        const auto digest = humtrace::sha256_hex(input);
        input.clear();
        input.seekg(0);
        humtrace::WavStreamReader reader(input);
        auto report = humtrace::analyze_wav(reader, humtrace::PipelineSettings{});
        report.input_path = argv[1];
        report.input_sha256 = digest;
        print_summary(report);
        if (argc == 4) {
            const std::string report_path = argv[3];
            const auto json = humtrace::serialize_report_json(report);
            std::ofstream report_file(report_path, std::ios::binary | std::ios::trunc);
            if (!report_file)
                throw std::runtime_error("could not create JSON report");
            report_file << json;
            if (!report_file)
                throw std::runtime_error("failed while writing JSON report");
            std::cout << "JSON report saved: " << report_path << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "HumTrace: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
