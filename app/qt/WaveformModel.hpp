#pragma once

#include "humtrace/view.hpp"

#include <QAbstractTableModel>

// A borrowed view of the existing bounded envelope. No per-bucket objects or audio reads.
// The owner clears this model before replacing the immutable analysis result.
class WaveformModel : public QAbstractTableModel {
    Q_OBJECT
public:
    using QAbstractTableModel::QAbstractTableModel;
    void setEnvelope(const humtrace::ChannelView* source, std::uint32_t sampleRate,
                     std::uint64_t analyzedSamples, std::uint16_t channel);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    const humtrace::ChannelView* source_{nullptr};
    std::uint32_t sampleRate_{0};
    std::uint64_t analyzedSamples_{0};
    std::uint16_t channel_{0};
};
