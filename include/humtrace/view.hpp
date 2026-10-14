#pragma once

#include "humtrace/spectrum.hpp"

#include <cstddef>
#include <vector>

namespace humtrace {

// Per-channel presentation data. Derived from analysis input; never used as DSP input.
struct ChannelView {
    std::vector<float> waveform_min; // fixed-bucket min/max envelope of the whole file
    std::vector<float> waveform_max;
    std::vector<std::vector<float>> spectrogram_dbfs; // [frame][row], log-spaced rows
    std::vector<double> row_frequency_hz;             // lower edge of each row
};

// Streaming min/max envelope with a bounded number of buckets.
class EnvelopeBuilder {
public:
    EnvelopeBuilder(std::size_t buckets, std::uint64_t total_samples);
    void add(const double* samples, std::size_t count);
    void finish(std::vector<float>& min_out, std::vector<float>& max_out) const;

private:
    std::size_t buckets_;
    std::uint64_t samples_per_bucket_;
    std::uint64_t seen_{};
    std::vector<float> min_, max_;
};

// Lower-edge frequencies of `rows` log-spaced rows from min_hz to the spectrum's Nyquist.
[[nodiscard]] std::vector<double> log_row_edges(const std::vector<SpectrumBin>& spectrum,
                                                std::size_t rows, double min_hz);

// Reduce a spectrum to one value per row (maximum dBFS of bins in the row; bins are
// never interpolated). -inf levels are floored at floor_dbfs.
[[nodiscard]] std::vector<float> reduce_to_rows(const std::vector<SpectrumBin>& spectrum,
                                                const std::vector<double>& row_edges,
                                                double floor_dbfs);

} // namespace humtrace
