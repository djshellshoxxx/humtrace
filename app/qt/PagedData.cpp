#include "PagedData.hpp"

PagedData::PagedData(std::shared_ptr<humtrace::DiskAnalysis> storage, std::size_t bytes) : storage_(std::move(storage)) {
    if (!storage_ || bytes < 65536) throw std::invalid_argument("paged cache needs at least 64 KiB");
    frames_.capacity = std::max<std::size_t>(1, bytes / 2 / 8192);
    const auto& views = storage_->views();
    const auto rows = views.empty() ? 0 : views[0].row_frequency_hz.size();
    rowEntryBytes_ = rows * sizeof(float) + 256;
    rows_.capacity = std::max<std::size_t>(1, bytes / 2 / rowEntryBytes_);
}
const humtrace::InterferenceFrame& PagedData::frame(std::size_t c, std::uint64_t f) {
    return frames_.get({c, f}, [&] { return storage_->frame(c, f); });
}
const std::vector<float>& PagedData::row(std::size_t c, std::uint64_t f, unsigned level) {
    return rows_.get({c, f, level}, [&] { return storage_->spectrogram_row(c, f, level); });
}
humtrace::ui::FindingRow PagedData::finding(std::size_t c, std::uint64_t index) const {
    const auto track = storage_->track(c, index);
    const auto& report = storage_->report();
    const auto rate = static_cast<double>(report.sample_rate_hz);
    return {static_cast<std::uint16_t>(c + 1), track.id,
        static_cast<double>(storage_->frame_start_sample(c, track.first_frame)) / rate,
        static_cast<double>(storage_->frame_start_sample(c, track.last_frame) + report.frame_size_samples) / rate,
        track.median_frequency_hz, track.median_level_dbfs, track.drift_hz_per_second,
        track.observed_frames, track.observed_frames + track.missing_frames, track.first_frame, track.last_frame};
}
std::size_t PagedData::cachedBytesUpperBound() const {
    return frames_.entries.size() * 8192 + rows_.entries.size() * rowEntryBytes_;
}
