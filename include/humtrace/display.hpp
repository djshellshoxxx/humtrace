// Purpose: bounded-size display products (waveform envelope, spectrogram) built
// during the single streaming analysis pass. Display only; not part of the JSON
// report and not used for any measurement.
#pragma once

#include "humtrace/spectrum.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace humtrace {

struct WaveformEnvelope {
    std::uint64_t samples_per_bucket{};
    std::vector<float> minimum;   // per bucket, normalized [-1, 1]
    std::vector<float> maximum;
};

// Log-frequency spectrogram. Each column max-holds `frames_per_column` frames;
// each row max-holds the FFT bins in [row_edges_hz[r], row_edges_hz[r+1]).
struct Spectrogram {
    std::size_t frames_per_column{};
    std::size_t rows{};
    std::vector<double> row_edges_hz;   // rows + 1 ascending edges
    std::vector<float> levels_dbfs;     // column-major: column * rows + row
    [[nodiscard]] std::size_t columns() const { return rows == 0 ? 0 : levels_dbfs.size() / rows; }
    [[nodiscard]] float at(std::size_t column, std::size_t row) const { return levels_dbfs[column * rows + row]; }
};

struct ChannelDisplay {
    std::uint16_t channel_number{};
    WaveformEnvelope waveform;
    Spectrogram spectrogram;
};

class WaveformBuilder {
public:
    // total_samples > 0, max_buckets > 0; throws std::invalid_argument otherwise.
    WaveformBuilder(std::uint64_t total_samples, std::size_t max_buckets);
    void add(double sample);
    [[nodiscard]] WaveformEnvelope take();
private:
    WaveformEnvelope out_;
    std::uint64_t index_{0};
    std::uint64_t total_;
};

class SpectrogramBuilder {
public:
    // Throws std::invalid_argument for zero sizes or a non-positive rate.
    SpectrogramBuilder(std::size_t total_frames, std::size_t max_columns, std::size_t rows,
                       double sample_rate_hz, std::size_t frame_size,
                       double min_frequency_hz = 10.0);
    void add_frame(const std::vector<SpectrumBin>& spectrum);  // size frame_size/2+1
    [[nodiscard]] Spectrogram take();
private:
    Spectrogram out_;
    std::vector<std::size_t> row_first_bin_, row_last_bin_;  // inclusive bin ranges
    std::size_t frame_in_column_{0};
    std::size_t bins_;
};

} // namespace humtrace
