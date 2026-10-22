#include "humtrace/compare.hpp"

#include "humtrace/json.hpp"
#include "humtrace/streaming.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace humtrace {
namespace {
ComparisonInput describe(const AnalysisReport& r) {
    return {r.input_path, r.input_sha256, r.sample_rate_hz, r.channels, r.duration_seconds};
}

using ChannelIndex = std::unordered_map<std::uint16_t, const ReportChannel*>;

ChannelIndex index_channels(const AnalysisReport& r) {
    ChannelIndex index;
    index.reserve(r.channel_results.size());
    for (const auto& c : r.channel_results)
        index.emplace(c.channel_number, &c);
    return index;
}

TrackRef ref(std::uint16_t channel, const Track& t) {
    return {channel, t.id, t.median_frequency_hz, t.median_level_dbfs};
}

using json::write_number;
void write_text(std::ostream& out, const std::string& value) { json::write_string(out, value); }

void write_input(std::ostream& out, const ComparisonInput& in) {
    out << "{\"path\": ";
    write_text(out, in.path);
    out << ", \"sha256\": ";
    if (in.sha256.empty()) out << "null"; else write_text(out, in.sha256);
    out << ", \"sample_rate_hz\": " << in.sample_rate_hz << ", \"channels\": " << in.channels
        << ", \"duration_seconds\": ";
    write_number(out, in.duration_seconds);
    out << '}';
}

void write_ref(std::ostream& out, const TrackRef& t) {
    out << "{\"channel\": " << t.channel << ", \"local_id\": " << t.local_id << ", \"median_frequency_hz\": ";
    write_number(out, t.median_frequency_hz);
    out << ", \"median_level_dbfs\": ";
    write_number(out, t.median_level_dbfs);
    out << '}';
}
} // namespace

ComparisonReport compare_reports(const AnalysisReport& a, const AnalysisReport& b,
                                 const CompareSettings& settings, const std::function<bool()>& cancelled) {
    if (!std::isfinite(settings.frequency_tolerance_hz) || settings.frequency_tolerance_hz < 0.0)
        throw std::invalid_argument("frequency tolerance must be finite and non-negative");
    auto checkpoint = [&] { if (cancelled && cancelled()) throw AnalysisCancelled(); };
    checkpoint();
    constexpr std::uint64_t max_tracks = 131072, max_search_pairs = 16 * 1024 * 1024;
    for (const auto* report : {&a, &b}) {
        std::uint64_t count = 0;
        for (const auto& channel : report->channel_results) {
            if (channel.tracks.size() > max_tracks - count)
                throw std::runtime_error("comparison track-summary memory limit exceeded");
            count += channel.tracks.size();
        }
    }
    std::uint64_t work = 0;
    ComparisonReport out;
    out.a = describe(a);
    out.b = describe(b);
    out.settings = settings;
    auto map = settings.channel_map;
    if (map.empty())
        for (std::uint32_t c = 1; c <= std::min(a.channels, b.channels); ++c) // 32-bit: 65535 channels must not wrap
            map.emplace_back(static_cast<std::uint16_t>(c), static_cast<std::uint16_t>(c));
    if (a.sample_rate_hz != b.sample_rate_hz)
        out.warnings.push_back("sample rates differ; no resampling was applied");
    if (a.frame_size_samples != b.frame_size_samples)
        out.warnings.push_back("analysis windows differ; frequency resolution is not equal");
    if (std::abs(a.duration_seconds - b.duration_seconds) > 1e-9)
        out.warnings.push_back("durations differ; no time alignment was applied");
    if (a.channels != b.channels && settings.channel_map.empty())
        out.warnings.push_back("channel counts differ; unmapped channels are reported as unmatched");

    {
        std::unordered_set<std::uint16_t> seen_a, seen_b;
        for (const auto& [ca, cb] : map)
            if (!seen_a.insert(ca).second || !seen_b.insert(cb).second)
                throw std::invalid_argument("channel map must pair each channel at most once");
    }
    const auto index_a = index_channels(a);
    const auto index_b = index_channels(b);
    out.settings.channel_map = map; // record the resolved pairing (identity expanded)
    std::unordered_set<std::uint16_t> used_a, used_b;
    for (const auto& [ca, cb] : map) {
        const auto ia = index_a.find(ca);
        const auto ib = index_b.find(cb);
        if (ia == index_a.end() || ib == index_b.end())
            throw std::invalid_argument("channel map references a channel that does not exist");
        const auto* chan_a = ia->second;
        const auto* chan_b = ib->second;
        used_a.insert(ca);
        used_b.insert(cb);
        checkpoint();
        if (!chan_b->tracks.empty() && chan_a->tracks.size() > (max_search_pairs - work) / chan_b->tracks.size())
            throw std::runtime_error("comparison search work limit exceeded");
        const auto pairs = chan_a->tracks.size() * chan_b->tracks.size();
        work += pairs;
        std::vector<std::tuple<double, std::size_t, std::size_t>> candidates;
        candidates.reserve(std::min(pairs, std::size_t{1} << 20));
        for (std::size_t i = 0; i < chan_a->tracks.size(); ++i) {
            checkpoint();
            for (std::size_t j = 0; j < chan_b->tracks.size(); ++j) {
                const double d = std::abs(chan_b->tracks[j].median_frequency_hz -
                                          chan_a->tracks[i].median_frequency_hz);
                if (d <= settings.frequency_tolerance_hz) {
                    if (candidates.size() >= (std::size_t{1} << 20))
                        throw std::runtime_error("comparison candidate memory limit exceeded");
                    candidates.emplace_back(d, i, j);
                }
            }
        }
        std::sort(candidates.begin(), candidates.end());
        checkpoint();
        std::vector<bool> taken_a(chan_a->tracks.size()), taken_b(chan_b->tracks.size());
        for (const auto& [d, i, j] : candidates) {
            if (taken_a[i] || taken_b[j]) continue;
            taken_a[i] = taken_b[j] = true;
            const auto& ta = chan_a->tracks[i];
            const auto& tb = chan_b->tracks[j];
            out.matches.push_back({ref(ca, ta), ref(cb, tb),
                                   tb.median_frequency_hz - ta.median_frequency_hz,
                                   tb.median_level_dbfs - ta.median_level_dbfs});
        }
        for (std::size_t i = 0; i < taken_a.size(); ++i)
            if (!taken_a[i]) out.unmatched_a.push_back(ref(ca, chan_a->tracks[i]));
        for (std::size_t j = 0; j < taken_b.size(); ++j)
            if (!taken_b[j]) out.unmatched_b.push_back(ref(cb, chan_b->tracks[j]));
    }
    // Tracks on channels outside the map stay visible as unmatched.
    for (const auto& c : a.channel_results)
        if (!used_a.contains(c.channel_number))
            for (const auto& t : c.tracks) out.unmatched_a.push_back(ref(c.channel_number, t));
    for (const auto& c : b.channel_results)
        if (!used_b.contains(c.channel_number))
            for (const auto& t : c.tracks) out.unmatched_b.push_back(ref(c.channel_number, t));
    return out;
}

AnalysisReport comparison_summary(const DiskAnalysis& storage, const std::function<bool()>& cancelled,
                                  std::uint64_t max_bytes) {
    const auto& metadata = storage.report();
    std::uint64_t required = static_cast<std::uint64_t>(metadata.channels) *
        (sizeof(ReportChannel) + sizeof(PsdSummary) + 20 * sizeof(PsdHarmonicMeasurement) + 256);
    if (required > max_bytes) throw std::runtime_error("comparison metadata memory limit exceeded");
    for (std::size_t c = 0; c < metadata.channels; ++c) {
        if (storage.track_count(c) > (max_bytes - required) / sizeof(Track))
            throw std::runtime_error("comparison track-summary memory limit exceeded");
        required += storage.track_count(c) * sizeof(Track);
    }
    auto summary = metadata;
    for (std::size_t c = 0; c < metadata.channels; ++c) {
        auto& tracks = summary.channel_results[c].tracks;
        tracks.reserve(static_cast<std::size_t>(storage.track_count(c)));
        for (std::uint64_t t = 0; t < storage.track_count(c); ++t) {
            if (cancelled && cancelled()) throw AnalysisCancelled();
            const auto stored = storage.track(c, t);
            Track track;
            track.id = stored.id; track.first_frame = stored.first_frame; track.last_frame = stored.last_frame;
            track.observed_frames = stored.observed_frames; track.missing_frames = stored.missing_frames;
            track.median_frequency_hz = stored.median_frequency_hz; track.median_level_dbfs = stored.median_level_dbfs;
            track.drift_hz_per_second = stored.drift_hz_per_second;
            tracks.push_back(std::move(track));
        }
    }
    return summary;
}

std::string serialize_comparison_json(const ComparisonReport& r) {
    std::ostringstream out;
    out << std::setprecision(17) << "{\n  \"schema_version\": \"comparison-1.0\",\n  \"application\": \"HumTrace\","
        << "\n  \"mode\": \"independent_summary\",\n  \"alignment\": {\"applied\": false, \"offset_seconds\": null},"
        << "\n  \"resampling\": {\"applied\": false},\n  \"settings\": {\"frequency_tolerance_hz\": ";
    write_number(out, r.settings.frequency_tolerance_hz);
    out << ", \"channel_map\": [";
    for (std::size_t i = 0; i < r.settings.channel_map.size(); ++i)
        out << (i ? ", " : "") << '[' << r.settings.channel_map[i].first << ", " << r.settings.channel_map[i].second << ']';
    out << "]},\n  \"inputs\": [";
    write_input(out, r.a);
    out << ", ";
    write_input(out, r.b);
    out << "],\n  \"matches\": [";
    for (std::size_t i = 0; i < r.matches.size(); ++i) {
        const auto& m = r.matches[i];
        out << (i ? ",\n    " : "\n    ") << "{\"a\": ";
        write_ref(out, m.a);
        out << ", \"b\": ";
        write_ref(out, m.b);
        out << ", \"delta_frequency_hz\": ";
        write_number(out, m.delta_frequency_hz);
        out << ", \"delta_level_db\": ";
        write_number(out, m.delta_level_db);
        out << '}';
    }
    out << "],\n  \"unmatched_a\": [";
    for (std::size_t i = 0; i < r.unmatched_a.size(); ++i) { out << (i ? ", " : ""); write_ref(out, r.unmatched_a[i]); }
    out << "],\n  \"unmatched_b\": [";
    for (std::size_t i = 0; i < r.unmatched_b.size(); ++i) { out << (i ? ", " : ""); write_ref(out, r.unmatched_b[i]); }
    out << "],\n  \"warnings\": [";
    for (std::size_t i = 0; i < r.warnings.size(); ++i) { out << (i ? ", " : ""); write_text(out, r.warnings[i]); }
    out << "],\n  \"interpretation\": {\"note\": \"Similar frequencies do not show a common device, environment, or location.\"}\n}\n";
    return out.str();
}

} // namespace humtrace
