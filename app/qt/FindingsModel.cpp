#include "FindingsModel.hpp"

namespace {
const char* kHeaders[] = {"Ch", "Track", "Start (s)", "End (s)", "Median freq (Hz)",
                          "Median level (dBFS)", "Coverage", "Drift (Hz/s)", "Kind"};
constexpr int kColumns = 9;
}

void FindingsModel::setRows(std::vector<humtrace::ui::FindingRow> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

int FindingsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}
int FindingsModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : kColumns; }

QVariant FindingsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& r = rows_[static_cast<std::size_t>(index.row())];
    if (role == Qt::TextAlignmentRole && index.column() >= 2 && index.column() <= 7)
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::UserRole) { // numeric key so explicit-column sorting is not lexical
        switch (index.column()) {
        case 0: return r.channel;
        case 1: return static_cast<qulonglong>(r.track_id);
        case 2: return r.start_seconds;
        case 3: return r.end_seconds;
        case 4: return r.median_frequency_hz;
        case 5: return r.median_level_dbfs;
        case 6: return r.coverage();
        case 7: return r.drift_hz_per_second;
        default: return QStringLiteral("Observation");
        }
    }
    if (role != Qt::DisplayRole && role != Qt::AccessibleTextRole) return {};
    switch (index.column()) {
    case 0: return r.channel;
    case 1: return static_cast<qulonglong>(r.track_id);
    case 2: return QString::number(r.start_seconds, 'f', 3);
    case 3: return QString::number(r.end_seconds, 'f', 3);
    case 4: return QString::number(r.median_frequency_hz, 'f', 3);
    case 5: return QString::number(r.median_level_dbfs, 'f', 2);
    case 6: return QString("%1 of %2 frames").arg(r.observed_frames).arg(r.spanned_frames);
    case 7: return QString::number(r.drift_hz_per_second, 'f', 4);
    case 8: return QStringLiteral("Observation");
    }
    return {};
}

QVariant FindingsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < kColumns)
        return QString::fromLatin1(kHeaders[section]);
    return {};
}
