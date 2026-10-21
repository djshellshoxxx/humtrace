#include "humtrace/compare.hpp"

#include "humtrace/music.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace humtrace {

TrackComparison compare_tracks(const std::vector<Track>& a, const std::vector<Track>& b,
                               double frequency_tolerance_hz) {
    if (!std::isfinite(frequency_tolerance_hz) || frequency_tolerance_hz < 0.0)
        throw std::invalid_argument("frequency tolerance must be finite and non-negative");
    struct Pair { double distance; std::size_t ia; std::size_t ib; };
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = 0; j < b.size(); ++j) {
            const double distance = std::abs(b[j].median_frequency_hz - a[i].median_frequency_hz);
            if (distance <= frequency_tolerance_hz)
                pairs.push_back({distance, i, j});
        }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) {
        return std::tie(x.distance, x.ia, x.ib) < std::tie(y.distance, y.ia, y.ib);
    });
    TrackComparison result;
    result.frequency_tolerance_hz = frequency_tolerance_hz;
    std::vector<bool> used_a(a.size(), false), used_b(b.size(), false);
    for (const auto& p : pairs) {
        if (used_a[p.ia] || used_b[p.ib])
            continue;
        used_a[p.ia] = used_b[p.ib] = true;
        const auto& ta = a[p.ia];
        const auto& tb = b[p.ib];
        result.matches.push_back({ta.id, tb.id, tb.median_frequency_hz - ta.median_frequency_hz,
                                  tb.median_level_dbfs - ta.median_level_dbfs,
                                  interval_cents(ta.median_frequency_hz, tb.median_frequency_hz)});
    }
    std::sort(result.matches.begin(), result.matches.end(),
              [](const TrackMatch& x, const TrackMatch& y) { return x.track_a_id < y.track_a_id; });
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!used_a[i]) result.unmatched_a.push_back(a[i].id);
    for (std::size_t j = 0; j < b.size(); ++j)
        if (!used_b[j]) result.unmatched_b.push_back(b[j].id);
    return result;
}

} // namespace humtrace
