#include "humtrace/streaming.hpp"
#include "humtrace/compare.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <random>

namespace fs = std::filesystem;
namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void integer(std::ostream& out, std::uint32_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 255));
}
}
int main() {
    std::random_device random;
    const auto directory = fs::temp_directory_path() / ("humtrace-stream-api-" + std::to_string(random()));
    require(fs::create_directory(directory), "private test directory created");
    const auto input = directory / "input.wav";
    {
        std::ofstream out(input, std::ios::binary);
        out << "RIFF"; integer(out, 36 + 32768, 4); out << "WAVEfmt "; integer(out, 16, 4);
        integer(out, 1, 2); integer(out, 1, 2); integer(out, 4096, 4); integer(out, 8192, 4);
        integer(out, 2, 2); integer(out, 16, 2); out << "data"; integer(out, 32768, 4);
        for (int i = 0; i < 16384; ++i) integer(out, i % 64 < 32 ? 8192U : static_cast<std::uint32_t>(-8192), 2);
    }
    const auto output = directory / "output.json";
    for (const auto minimum : {std::size_t{1}, std::size_t{99}}) {
        // Ambiguities remain reviewable even when every short segment is suppressed.
        humtrace::PipelineSettings settings;
        settings.tracker.fixed_hz_tolerance = 2000.0;
        settings.tracker.bin_fraction = 0.0;
        settings.tracker.min_observed_frames = minimum;
        settings.max_peaks_per_frame = 4;
        const auto retained = humtrace::analyze_file(input.string(), settings, {}, false).report;
        require(!retained.channel_results[0].ambiguities.empty(), "fixture creates candidate ambiguity");
        const auto paged = humtrace::analyze_file_paged(input.string(), settings);
        require(paged->track_count(0) == retained.channel_results[0].tracks.size(),
                "paged track index preserves suppression");
        const auto target = directory / "ambiguities.json";
        paged->export_json(target.string(), false, true);
        std::ifstream exported(target, std::ios::binary);
        std::ostringstream contents;
        contents << exported.rdbuf();
        require(contents.str() == humtrace::serialize_report_json(retained),
                "paged JSON preserves ambiguity events and segment references exactly");
        for (std::size_t i = 0; i < retained.channel_results[0].tracks.size(); ++i) {
            const auto stored = paged->track(0, i);
            const auto& track = retained.channel_results[0].tracks[i];
            require(stored.start_ambiguity_id == track.start_ambiguity_id &&
                    stored.end_ambiguity_id == track.end_ambiguity_id && stored.end_reason == track.end_reason,
                    "paged inspector preserves ambiguity boundaries");
        }
        exported.close();
        fs::remove(target);
    }
    for (const auto stage : {humtrace::Stage::Hashing, humtrace::Stage::Analyzing, humtrace::Stage::Finalizing}) {
        bool cancel = false;
        humtrace::SessionCallbacks callbacks;
        callbacks.progress = [&](humtrace::Stage current, double) { if (current == stage) cancel = true; };
        callbacks.cancelled = [&] { return cancel; };
        bool cancelled = false;
        try { (void)humtrace::stream_file_report(input.string(), output.string(), {}, callbacks); }
        catch (const humtrace::AnalysisCancelled&) { cancelled = true; }
        require(cancelled, "cancel supported in hash, analysis and report finalization");
        require(!fs::exists(output), "cancellation never publishes partial report");
        std::size_t entries = 0;
        for (const auto& entry : fs::directory_iterator(directory)) { (void)entry; ++entries; }
        require(entries == 1, "all scratch and sibling temporary files removed");
    }
    { // The second finalizing notification precedes atomic JSON publication.
        unsigned finalizing = 0;
        humtrace::SessionCallbacks callbacks;
        callbacks.progress = [&](humtrace::Stage stage, double) { if (stage == humtrace::Stage::Finalizing) ++finalizing; };
        callbacks.cancelled = [&] { return finalizing >= 2; };
        bool cancelled = false;
        try { (void)humtrace::stream_file_report(input.string(), output.string(), {}, callbacks); }
        catch (const humtrace::AnalysisCancelled&) { cancelled = true; }
        require(cancelled && !fs::exists(output), "cancel at report publication removes temporary output");
        std::size_t entries = 0;
        for (const auto& entry : fs::directory_iterator(directory)) { (void)entry; ++entries; }
        require(entries == 1, "publication cancellation removes all private storage");
    }
    humtrace::StreamingLimits limits;
    limits.min_free_disk_bytes = std::numeric_limits<std::uint64_t>::max();
    bool refused = false;
    try { (void)humtrace::stream_file_report(input.string(), output.string(), {}, {}, limits); }
    catch (const std::runtime_error& error) { refused = std::string(error.what()).find("free disk") != std::string::npos; }
    require(refused && !fs::exists(output), "insufficient free space is rejected before report publication");
    { // Disk-backed random access and pyramid values preserve the small-file API.
        const auto plain = humtrace::analyze_file(input.string(), {}, {}, true);
        const auto paged = humtrace::analyze_file_paged(input.string(), {});
        require(paged->report().channel_results[0].frames.empty(), "paged report retains no frame tree");
        require(paged->views()[0].spectrogram_dbfs.empty(), "paged view retains no spectrogram tree");
        require(paged->views()[0].waveform_min == plain.views[0].waveform_min, "paged waveform agrees");
        require(paged->frame_count(0) == plain.report.channel_results[0].frames.size(), "all frame indexes retained");
        for (std::uint64_t f = 0; f < paged->frame_count(0); ++f) {
            std::ostringstream expected;
            expected << std::setprecision(17);
            humtrace::write_frame_json(expected, plain.report.channel_results[0].frames[static_cast<std::size_t>(f)], 4096);
            require(paged->frame_json(0, f) == expected.str(), "random-access frame exactly agrees");
            const auto typed = paged->frame(0, f);
            require(typed.start_sample == plain.report.channel_results[0].frames[static_cast<std::size_t>(f)].start_sample &&
                    typed.strongest_peak.level_dbfs == plain.report.channel_results[0].frames[static_cast<std::size_t>(f)].strongest_peak.level_dbfs,
                    "typed frame preserves original measurement values without JSON projection");
            require(paged->spectrogram_row(0, f) == plain.views[0].spectrogram_dbfs[static_cast<std::size_t>(f)], "base spectrogram retains every value");
        }
        auto expected = plain.views[0].spectrogram_dbfs;
        for (unsigned level = 1; expected.size() > 1; ++level) {
            std::vector<std::vector<float>> reduced;
            for (std::size_t i = 0; i < expected.size(); i += 2) {
                auto row = expected[i];
                if (i + 1 < expected.size()) for (std::size_t r = 0; r < row.size(); ++r) row[r] = std::max(row[r], expected[i+1][r]);
                reduced.push_back(row);
            }
            expected = std::move(reduced);
            for (std::size_t i = 0; i < expected.size(); ++i)
                require(paged->spectrogram_row(0, i, level) == expected[i], "pyramid is an exact maximum reduction");
        }
        require(paged->track_count(0) == plain.report.channel_results[0].tracks.size(), "all track summaries indexed");
        for (std::size_t t = 0; t < plain.report.channel_results[0].tracks.size(); ++t) {
            const auto stored = paged->track(0, t);
            const auto& reference = plain.report.channel_results[0].tracks[t];
            require(stored.id == reference.id && stored.first_frame == reference.first_frame &&
                    stored.last_frame == reference.last_frame && stored.observed_frames == reference.observed_frames &&
                    stored.median_frequency_hz == reference.median_frequency_hz &&
                    stored.median_level_dbfs == reference.median_level_dbfs &&
                    stored.drift_hz_per_second == reference.drift_hz_per_second,
                    "paged track metadata agrees exactly");
            require(stored.evidence.measured && stored.evidence.sample_peak == plain.report.channel_results[0].frames[0].levels->sample_peak_dbfs,
                    "inspector evidence is retained without an in-memory timeline");
        }
        const auto summary = humtrace::comparison_summary(*paged);
        require(summary.channel_results[0].frames.empty() && summary.channel_results[0].tracks[0].observations.empty(),
                "comparison materializes only summaries");
        require(humtrace::serialize_comparison_json(humtrace::compare_reports(summary, summary)) ==
                humtrace::serialize_comparison_json(humtrace::compare_reports(plain.report, plain.report)),
                "disk-backed and in-memory comparison reports agree exactly");
        const auto exported = directory / "paged-export.json";
        paged->export_json(exported.string(), false, true);
        paged->export_json(exported.string(), true, true); // immutable storage supports repeatable export
        bool out_of_range = false;
        try { (void)paged->frame_json(0, paged->frame_count(0)); }
        catch (const std::out_of_range&) { out_of_range = true; }
        require(out_of_range, "paged frame bounds enforced");
    }
    fs::remove_all(directory);
    std::cout << "Streaming API: cancellation at all stages, insufficient disk and cleanup passed.\n";
}
