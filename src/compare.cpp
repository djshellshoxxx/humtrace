#include "humtrace/compare.hpp"

#include "humtrace/version.hpp"
#include "json_writer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace humtrace {
namespace {
double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    const auto mid = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), mid, values.end());
    return *mid;
}
} // namespace

FileSummary summarize(const AnalysisReport& report, std::size_t min_support) {
    FileSummary out;
    out.path = report.input_path;
    out.sha256 = report.input_sha256;
    out.sample_rate_hz = report.sample_rate_hz;
    out.frame_size_samples = report.frame_size_samples;
    out.track_tolerance_hz = report.track_tolerance_hz;
    for (const auto& channel : report.channel_results) {
        ChannelSummary s;
        s.channel_number = channel.channel_number;
        s.frames = channel.frames.size();
        std::vector<double> hz, db;
        for (const auto& frame : channel.frames) {
            if (frame.mains_50.supporting_harmonics >= min_support) ++s.frames_50_hz_supported;
            if (frame.mains_60.supporting_harmonics >= min_support) ++s.frames_60_hz_supported;
            if (std::isfinite(frame.strongest_peak.level_dbfs)) {
                hz.push_back(frame.strongest_peak.frequency_hz);
                db.push_back(frame.strongest_peak.level_dbfs);
            }
        }
        s.median_peak_hz = median(hz);
        s.median_peak_dbfs = median(db);
        s.tracks = channel.tracks;
        out.channels.push_back(s);
    }
    return out;
}

std::vector<SharedTone> find_shared_tones(const std::vector<FileSummary>& files,
                                          double min_persistence) {
    struct Item { double hz; std::size_t file; };
    std::vector<Item> items;
    double tolerance = 0.0;
    for (std::size_t f = 0; f < files.size(); ++f) {
        tolerance = std::max(tolerance, files[f].track_tolerance_hz);
        for (const auto& c : files[f].channels)
            for (const auto& t : c.tracks)
                if (t.persistence >= min_persistence && std::isfinite(t.median_frequency_hz))
                    items.push_back({t.median_frequency_hz, f});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.hz != b.hz ? a.hz < b.hz : a.file < b.file;
    });
    std::vector<SharedTone> shared;
    // Single-linkage clustering on sorted frequencies, bounded by distance to the cluster start
    // so a dense chain of tones cannot merge an unbounded range.
    for (std::size_t i = 0; i < items.size();) {
        std::size_t j = i + 1;
        while (j < items.size() && items[j].hz - items[j - 1].hz <= tolerance &&
               items[j].hz - items[i].hz <= 2.0 * tolerance)
            ++j;
        std::vector<std::size_t> member_files;
        std::vector<double> hz;
        for (std::size_t k = i; k < j; ++k) {
            member_files.push_back(items[k].file);
            hz.push_back(items[k].hz);
        }
        std::sort(member_files.begin(), member_files.end());
        member_files.erase(std::unique(member_files.begin(), member_files.end()), member_files.end());
        if (member_files.size() >= 2) {
            std::sort(hz.begin(), hz.end());
            const double mid = hz.size() % 2 ? hz[hz.size() / 2]
                                             : (hz[hz.size() / 2 - 1] + hz[hz.size() / 2]) / 2.0;
            shared.push_back({mid, member_files, nearest_mains(mid, tolerance)});
        }
        i = j;
    }
    return shared;
}

std::string format_comparison(const std::vector<FileSummary>& files) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(2);
    o << "HumTrace comparison (measurements only; no source attribution)\n"
      << "Supported = frames with >=3 harmonics over threshold and prominence.\n";
    for (const auto& f : files) {
        o << "\n" << f.path << "\n  sha256 " << f.sha256 << '\n';
        for (const auto& c : f.channels)
            o << "  ch" << c.channel_number << ": frames " << c.frames << ", 50 Hz supported "
              << c.frames_50_hz_supported << ", 60 Hz supported " << c.frames_60_hz_supported
              << ", median peak " << c.median_peak_hz << " Hz / " << c.median_peak_dbfs
              << " dBFS, tone tracks " << c.tracks.size() << '\n';
    }
    const auto shared = find_shared_tones(files);
    o << "\nTones tracked in 2+ files (persistence >= 0.50): " << shared.size() << '\n';
    for (const auto& t : shared) {
        o << "  " << t.frequency_hz << " Hz in files";
        for (const auto f : t.files) o << ' ' << f + 1;
        if (t.mains.nominal_hz > 0.0)
            o << " (near " << t.mains.harmonic << " x " << t.mains.nominal_hz << " Hz)";
        o << '\n';
    }
    o << "Shared frequencies are measurements; they do not show a common source.\n";
    return o.str();
}

std::string serialize_comparison_json(const std::vector<FileSummary>& files) {
    using json::write_number;
    using json::write_string;
    std::ostringstream o;
    o << std::setprecision(17) << "{\n  \"comparison_schema_version\": \"1.0\",\n"
      << "  \"application\": \"HumTrace\",\n  \"application_version\": ";
    write_string(o, kVersion);
    o << ",\n  \"files\": [";
    for (std::size_t f = 0; f < files.size(); ++f) {
        const auto& file = files[f];
        o << (f ? ",\n    " : "\n    ") << "{\"index\": " << f + 1 << ", \"path\": ";
        write_string(o, file.path);
        o << ", \"sha256\": ";
        write_string(o, file.sha256);
        o << ", \"sample_rate_hz\": " << file.sample_rate_hz
          << ", \"frame_size_samples\": " << file.frame_size_samples << ", \"track_tolerance_hz\": ";
        write_number(o, file.track_tolerance_hz);
        o << ", \"channels\": [";
        for (std::size_t c = 0; c < file.channels.size(); ++c) {
            const auto& ch = file.channels[c];
            o << (c ? ", " : "") << "{\"channel_number\": " << ch.channel_number
              << ", \"frames\": " << ch.frames << ", \"frames_50_hz_supported\": " << ch.frames_50_hz_supported
              << ", \"frames_60_hz_supported\": " << ch.frames_60_hz_supported << ", \"median_peak_hz\": ";
            write_number(o, ch.median_peak_hz);
            o << ", \"median_peak_dbfs\": ";
            write_number(o, ch.median_peak_dbfs);
            o << ", \"track_frequencies_hz\": [";
            for (std::size_t t = 0; t < ch.tracks.size(); ++t) {
                if (t) o << ", ";
                write_number(o, ch.tracks[t].median_frequency_hz);
            }
            o << "]}";
        }
        o << "]}";
    }
    o << "\n  ],\n  \"shared_tones\": [";
    const auto shared = find_shared_tones(files);
    for (std::size_t i = 0; i < shared.size(); ++i) {
        const auto& t = shared[i];
        o << (i ? ",\n    " : "\n    ") << "{\"frequency_hz\": ";
        write_number(o, t.frequency_hz);
        o << ", \"files\": [";
        for (std::size_t k = 0; k < t.files.size(); ++k) o << (k ? ", " : "") << t.files[k] + 1;
        o << "], \"near_mains_multiple\": ";
        if (t.mains.nominal_hz > 0.0) {
            o << "{\"nominal_hz\": ";
            write_number(o, t.mains.nominal_hz);
            o << ", \"harmonic\": " << t.mains.harmonic << ", \"deviation_hz\": ";
            write_number(o, t.mains.deviation_hz);
            o << '}';
        } else {
            o << "null";
        }
        o << '}';
    }
    if (!shared.empty()) o << "\n  ";
    o << "],\n  \"settings\": {\"min_supporting_harmonics\": 3, \"shared_tone_min_persistence\": 0.5},\n"
      << "  \"interpretation\": {\"source_classification\": null, \"note\": "
      << "\"Shared frequencies are measurements; they do not show a common source, device or location.\"}\n}\n";
    return o.str();
}

} // namespace humtrace
