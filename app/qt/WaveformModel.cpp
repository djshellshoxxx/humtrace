#include "WaveformModel.hpp"

#include <algorithm>
#include <limits>

namespace {
constexpr int kColumns = 7;
const char* kHeaders[] = {"Channel", "Start sample (inclusive)", "End sample (exclusive)",
                         "Start time (s)", "End time (s)", "Minimum (normalized FS)", "Maximum (normalized FS)"};
}

void WaveformModel::setEnvelope(const humtrace::ChannelView* source, std::uint32_t sampleRate,
                                std::uint64_t analyzedSamples, std::uint16_t channel) {
    beginResetModel();
    source_ = source;
    sampleRate_ = sampleRate;
    analyzedSamples_ = analyzedSamples;
    channel_ = channel;
    endResetModel();
}

int WaveformModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !source_ || sampleRate_ == 0 || analyzedSamples_ == 0 || source_->waveform_samples_per_bucket == 0)
        return 0;
    const auto buckets = (analyzedSamples_ - 1) / source_->waveform_samples_per_bucket + 1;
    const auto count = std::min({static_cast<std::uint64_t>(source_->waveform_min.size()),
        static_cast<std::uint64_t>(source_->waveform_max.size()), buckets,
        static_cast<std::uint64_t>(std::numeric_limits<int>::max())});
    return static_cast<int>(count);
}

int WaveformModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : kColumns; }

QVariant WaveformModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.model() != this || index.row() >= rowCount() || index.column() >= kColumns) return {};
    if (role == Qt::TextAlignmentRole) return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::AccessibleDescriptionRole || role == Qt::ToolTipRole)
        return tr("Stored min/max envelope bucket of decoded samples; this is a presentation summary, not individual raw audio samples. End sample and end time are exclusive.");
    if (role == Qt::AccessibleTextRole)
        return tr("%1: %2; channel %3, envelope bucket %4")
            .arg(headerData(index.column(), Qt::Horizontal, Qt::DisplayRole).toString(), data(index, Qt::DisplayRole).toString())
            .arg(channel_).arg(index.row() + 1);
    if (role != Qt::DisplayRole && role != Qt::UserRole) return {};
    const auto row = static_cast<std::size_t>(index.row());
    const auto start = static_cast<std::uint64_t>(row) * source_->waveform_samples_per_bucket;
    const auto end = start + std::min(source_->waveform_samples_per_bucket, analyzedSamples_ - start);
    switch (index.column()) {
    case 0: return channel_;
    case 1: return static_cast<qulonglong>(start);
    case 2: return static_cast<qulonglong>(end);
    case 3: case 4: {
        const double time = static_cast<double>(index.column() == 3 ? start : end) / sampleRate_;
        return role == Qt::UserRole ? QVariant(time) : QVariant(QString::number(time, 'g', std::numeric_limits<double>::max_digits10));
    }
    case 5: case 6: {
        const double value = index.column() == 5 ? source_->waveform_min[row] : source_->waveform_max[row];
        return role == Qt::UserRole ? QVariant(value) : QVariant(QString::number(value, 'g', std::numeric_limits<float>::max_digits10));
    }
    }
    return {};
}

QVariant WaveformModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole && role != Qt::AccessibleTextRole) return {};
    if (orientation == Qt::Horizontal && section >= 0 && section < kColumns) return tr(kHeaders[section]);
    if (orientation == Qt::Vertical && section >= 0 && section < rowCount()) return section + 1;
    return {};
}
