#include "humtrace/spectrum.hpp"
#include "humtrace/report.hpp"
#include "humtrace/wav.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2 && (argc != 4 || std::string(argv[2]) != "--json")) {
        std::cerr << "Usage: humtrace <input.wav> [--json <report.json>]\n";
        return 2;
    }
    try {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input)
            throw std::runtime_error("could not open input file");
        const auto audio = humtrace::decode_wav(input);
        const auto frames = audio.frame_count();
        if (frames < 4)
            throw std::runtime_error("audio is too short to analyze");
        std::size_t window_size = 1;
        const auto frame_limit = std::min<std::uint64_t>(
            frames, std::max<std::uint64_t>(4, std::min<std::uint64_t>(audio.sample_rate_hz, 65536)));
        while (window_size <= frame_limit / 2)
            window_size *= 2;
        const std::size_t hop_size = window_size / 2;
        const auto bin_spacing = static_cast<double>(audio.sample_rate_hz) /
                                 static_cast<double>(window_size);
        humtrace::AnalysisReport report;
        report.input_path = argv[1];
        report.sample_rate_hz = audio.sample_rate_hz;
        report.channels = audio.channels;
        report.bit_depth = audio.bit_depth;
        report.format_code = audio.format_code;
        report.duration_seconds = static_cast<double>(frames) / audio.sample_rate_hz;
        report.frame_size_samples = window_size;
        report.hop_size_samples = hop_size;

        std::cout << std::fixed << std::setprecision(3)
                  << "HumTrace analysis (" << static_cast<double>(window_size) /
                         audio.sample_rate_hz << " s per frame)\n"
                  << "Sample rate: " << audio.sample_rate_hz << " Hz\n"
                  << "Channels: " << audio.channels << "\n"
                  << "Bit depth: " << audio.bit_depth << "\n"
                  << "Duration: " << static_cast<double>(frames) / audio.sample_rate_hz << " s\n"
                  << "FFT bins: " << window_size / 2 + 1 << " (" << bin_spacing << " Hz/bin)\n"
                  << "Timeline hop: " << static_cast<double>(hop_size) / audio.sample_rate_hz << " s\n";

        for (std::uint16_t channel = 0; channel < audio.channels; ++channel) {
            std::vector<double> samples(static_cast<std::size_t>(frames));
            for (std::size_t frame = 0; frame < samples.size(); ++frame)
                samples[frame] = audio.interleaved_samples[frame * audio.channels + channel];
            const auto timeline = humtrace::analyze_interference_timeline(
                samples, audio.sample_rate_hz, window_size, hop_size);
            std::cout << "Channel " << (channel + 1) << " measured frames: " << timeline.size() << '\n';
            for (const auto& frame : timeline) {
                const auto start_seconds = static_cast<double>(frame.start_sample) / audio.sample_rate_hz;
                const auto& peak = frame.strongest_peak;
                std::cout << "  " << start_seconds << " s: ";
                if (peak.frequency_hz == 0.0)
                    std::cout << "no non-DC energy";
                else
                    std::cout << "strongest non-DC component " << peak.frequency_hz << " Hz, "
                              << peak.level_dbfs << " dBFS";
                std::cout << "; "
                          << "50 Hz bins over -60 dBFS and 10 dB local prominence: "
                          << frame.mains_50.supporting_harmonics << '/' << frame.mains_50.harmonics.size()
                          << "; 60 Hz: " << frame.mains_60.supporting_harmonics << '/'
                          << frame.mains_60.harmonics.size() << '\n';
            }
            report.channel_results.push_back({static_cast<std::uint16_t>(channel + 1), timeline});
        }
        std::cout << "Interpretation: spectral components are measurements, not source attribution.\n";
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
