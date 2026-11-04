#include "humtrace/display.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace humtrace {

WaveformBuilder::WaveformBuilder(std::uint64_t total_samples, std::size_t max_buckets)
    : total_(total_samples) {
    if (total_samples == 0 || max_buckets == 0)
        throw std::invalid_argument("waveform needs samples and buckets");
    out_.samples_per_bucket = (total_samples + max_buckets - 1) / max_buckets;
    const auto buckets = static_cast<std::size_t>((total_samples + out_.samples_per_bucket - 1) /
                                                  out_.samples_per_bucket);
    out_.minimum.assign(buckets, std::numeric_limits<float>::infinity());
    out_.maximum.assign(buckets, -std::numeric_limits<float>::infinity());
}

void WaveformBuilder::add(double sample) {
    if (index_ >= total_)
        return;  // ignore samples past the declared total
    const auto bucket = static_cast<std::size_t>(index_ / out_.samples_per_bucket);
    const auto value = static_cast<float>(std::clamp(std::isfinite(sample) ? sample : 0.0, -1.0, 1.0));
    out_.minimum[bucket] = std::min(out_.minimum[bucket], value);
    out_.maximum[bucket] = std::max(out_.maximum[bucket], value);
    ++index_;
}

WaveformEnvelope WaveformBuilder::take() {
    for (std::size_t i = 0; i < out_.minimum.size(); ++i)
        if (out_.minimum[i] > out_.maximum[i])
            out_.minimum[i] = out_.maximum[i] = 0.0F;  // bucket never filled
    return std::move(out_);
}

SpectrogramBuilder::SpectrogramBuilder(std::size_t total_frames, std::size_t max_columns,
                                       std::size_t rows, double sample_rate_hz,
                                       std::size_t frame_size, double min_frequency_hz)
    : bins_(frame_size / 2 + 1) {
    if (total_frames == 0 || max_columns == 0 || rows == 0 || frame_size < 4 ||
        !std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0)
        throw std::invalid_argument("invalid spectrogram settings");
    out_.frames_per_column = (total_frames + max_columns - 1) / max_columns;
    out_.rows = rows;
    const double bin_hz = sample_rate_hz / static_cast<double>(frame_size);
    const double nyquist = sample_rate_hz / 2.0;
    const double lo = std::clamp(std::max(min_frequency_hz, bin_hz), bin_hz, nyquist / 2.0);
    out_.row_edges_hz.resize(rows + 1);
    for (std::size_t r = 0; r <= rows; ++r)
        out_.row_edges_hz[r] = lo * std::pow(nyquist / lo, static_cast<double>(r) / static_cast<double>(rows));
    row_first_bin_.resize(rows);
    row_last_bin_.resize(rows);
    for (std::size_t r = 0; r < rows; ++r) {
        auto first = static_cast<std::size_t>(std::ceil(out_.row_edges_hz[r] / bin_hz));
        auto last_d = std::ceil(out_.row_edges_hz[r + 1] / bin_hz) - 1.0;
        auto last = last_d < 0.0 ? 0 : static_cast<std::size_t>(last_d);
        if (r + 1 == rows)
            last = bins_ - 1;  // include Nyquist
        if (last < first) {    // row narrower than a bin: use the bin nearest its centre
            const double centre = std::sqrt(out_.row_edges_hz[r] * out_.row_edges_hz[r + 1]);
            first = last = static_cast<std::size_t>(std::llround(centre / bin_hz));
        }
        row_first_bin_[r] = std::min(first, bins_ - 1);
        row_last_bin_[r] = std::min(last, bins_ - 1);
    }
    const auto columns = (total_frames + out_.frames_per_column - 1) / out_.frames_per_column;
    out_.levels_dbfs.reserve(columns * rows);
}

void SpectrogramBuilder::add_frame(const std::vector<SpectrumBin>& spectrum) {
    if (spectrum.size() != bins_)
        throw std::invalid_argument("spectrum size does not match the spectrogram frame size");
    if (frame_in_column_ == 0)
        out_.levels_dbfs.insert(out_.levels_dbfs.end(), out_.rows, -std::numeric_limits<float>::infinity());
    float* column = out_.levels_dbfs.data() + (out_.levels_dbfs.size() - out_.rows);
    for (std::size_t r = 0; r < out_.rows; ++r) {
        double best = -std::numeric_limits<double>::infinity();
        for (std::size_t b = row_first_bin_[r]; b <= row_last_bin_[r]; ++b)
            best = std::max(best, spectrum[b].level_dbfs);
        column[r] = std::max(column[r], static_cast<float>(best));
    }
    frame_in_column_ = (frame_in_column_ + 1) % out_.frames_per_column;
}

Spectrogram SpectrogramBuilder::take() { return std::move(out_); }

} // namespace humtrace
