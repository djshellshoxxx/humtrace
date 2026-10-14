#include "humtrace/view.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace humtrace {

EnvelopeBuilder::EnvelopeBuilder(std::size_t buckets, std::uint64_t total_samples)
    : buckets_(buckets), samples_per_bucket_(1), min_(buckets, 0.0F), max_(buckets, 0.0F) {
    if (buckets == 0)
        throw std::invalid_argument("envelope needs at least one bucket");
    samples_per_bucket_ = std::max<std::uint64_t>(1, (total_samples + buckets - 1) / buckets);
}

void EnvelopeBuilder::add(const double* samples, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i, ++seen_) {
        const auto bucket = static_cast<std::size_t>(seen_ / samples_per_bucket_);
        if (bucket >= buckets_)
            return;
        const auto value = static_cast<float>(samples[i]);
        if (seen_ % samples_per_bucket_ == 0) {
            min_[bucket] = max_[bucket] = value;
        } else {
            min_[bucket] = std::min(min_[bucket], value);
            max_[bucket] = std::max(max_[bucket], value);
        }
    }
}

void EnvelopeBuilder::finish(std::vector<float>& min_out, std::vector<float>& max_out) const {
    const auto used = static_cast<std::size_t>(
        std::min<std::uint64_t>(buckets_, (seen_ + samples_per_bucket_ - 1) / samples_per_bucket_));
    min_out.assign(min_.begin(), min_.begin() + static_cast<std::ptrdiff_t>(used));
    max_out.assign(max_.begin(), max_.begin() + static_cast<std::ptrdiff_t>(used));
}

std::vector<double> log_row_edges(const std::vector<SpectrumBin>& spectrum, std::size_t rows,
                                  double min_hz) {
    if (spectrum.size() < 3 || rows == 0 || !(min_hz > 0.0) ||
        min_hz >= spectrum.back().frequency_hz)
        throw std::invalid_argument("invalid spectrogram row settings");
    const double ratio = std::pow(spectrum.back().frequency_hz / min_hz, 1.0 / static_cast<double>(rows));
    std::vector<double> edges(rows + 1);
    for (std::size_t i = 0; i <= rows; ++i)
        edges[i] = min_hz * std::pow(ratio, static_cast<double>(i));
    return edges;
}

std::vector<float> reduce_to_rows(const std::vector<SpectrumBin>& spectrum,
                                  const std::vector<double>& row_edges, double floor_dbfs) {
    if (spectrum.size() < 3 || row_edges.size() < 2)
        throw std::invalid_argument("invalid spectrogram reduction input");
    const double spacing = spectrum[1].frequency_hz - spectrum[0].frequency_hz;
    const auto last = spectrum.size() - 1;
    std::vector<float> out(row_edges.size() - 1, static_cast<float>(floor_dbfs));
    for (std::size_t row = 0; row + 1 < row_edges.size(); ++row) {
        auto first = static_cast<std::size_t>(std::ceil(row_edges[row] / spacing));
        auto end = static_cast<std::size_t>(std::ceil(row_edges[row + 1] / spacing));
        first = std::clamp<std::size_t>(first, 1, last);
        end = std::clamp<std::size_t>(end, 1, last + 1);
        if (end <= first) { // row narrower than one bin: use the nearest bin, no interpolation
            first = std::clamp<std::size_t>(static_cast<std::size_t>(
                std::llround((row_edges[row] + row_edges[row + 1]) / 2.0 / spacing)), 1, last);
            end = first + 1;
        }
        double best = floor_dbfs;
        for (std::size_t bin = first; bin < end; ++bin)
            if (std::isfinite(spectrum[bin].level_dbfs))
                best = std::max(best, spectrum[bin].level_dbfs);
        out[row] = static_cast<float>(best);
    }
    return out;
}

} // namespace humtrace
