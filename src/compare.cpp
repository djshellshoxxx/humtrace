#include "humtrace/compare.hpp"

#include "humtrace/json.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace humtrace {
namespace {
ComparisonInput describe(const AnalysisReport& r) {
    return {r.input_path, r.input_sha256, r.sample_rate_hz, r.channels, r.duration_seconds};
}

const ReportChannel* find_channel(const AnalysisReport& r, std::uint16_t number) {
    for (const auto& c : r.channel_results)
        if (c.channel_number == number)
            return &c;
    return nullptr;
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
                                 const CompareSettings& settings) {
    if (!std::isfinite(settings.frequency_tolerance_hz) || settings.frequency_tolerance_hz < 0.0)
        throw std::invalid_argument("frequency tolerance must be finite and non-negative");
    ComparisonReport out;
    out.a = describe(a);
    out.b = describe(b);
    out.settings = settings;
    auto map = settings.channel_map;
    if (map.empty())
        for (std::uint16_t c = 1; c <= std::min(a.channels, b.channels); ++c)
            map.emplace_back(c, c);
    if (a.sample_rate_hz != b.sample_rate_hz)
        out.warnings.push_back("sample rates differ; no resampling was applied");
    if (a.frame_size_samples != b.frame_size_samples)
        out.warnings.push_back("analysis windows differ; frequency resolution is not equal");
    if (std::abs(a.duration_seconds - b.duration_seconds) > 1e-9)
        out.warnings.push_back("durations differ; no time alignment was applied");
    if (a.channels != b.channels && settings.channel_map.empty())
        out.warnings.push_back("channel counts differ; unmapped channels are reported as unmatched");

    for (std::size_t i = 0; i < map.size(); ++i)
        for (std::size_t j = i + 1; j < map.size(); ++j)
            if (map[i].first == map[j].first || map[i].second == map[j].second)
                throw std::invalid_argument("channel map must pair each channel at most once");
    std::vector<std::uint16_t> used_a, used_b;
    for (const auto& [ca, cb] : map) {
        const auto* chan_a = find_channel(a, ca);
        const auto* chan_b = find_channel(b, cb);
        if (!chan_a || !chan_b)
            throw std::invalid_argument("channel map references a channel that does not exist");
        used_a.push_back(ca);
        used_b.push_back(cb);
        std::vector<std::tuple<double, std::size_t, std::size_t>> candidates;
        for (std::size_t i = 0; i < chan_a->tracks.size(); ++i)
            for (std::size_t j = 0; j < chan_b->tracks.size(); ++j) {
                const double d = std::abs(chan_b->tracks[j].median_frequency_hz -
                                          chan_a->tracks[i].median_frequency_hz);
                if (d <= settings.frequency_tolerance_hz)
                    candidates.emplace_back(d, i, j);
            }
        std::sort(candidates.begin(), candidates.end());
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
        if (std::find(used_a.begin(), used_a.end(), c.channel_number) == used_a.end())
            for (const auto& t : c.tracks) out.unmatched_a.push_back(ref(c.channel_number, t));
    for (const auto& c : b.channel_results)
        if (std::find(used_b.begin(), used_b.end(), c.channel_number) == used_b.end())
            for (const auto& t : c.tracks) out.unmatched_b.push_back(ref(c.channel_number, t));
    return out;
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
