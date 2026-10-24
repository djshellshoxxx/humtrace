#pragma once

#include "../presentation/findings.hpp"

#include <QAbstractTableModel>

class FindingsModel : public QAbstractTableModel {
    Q_OBJECT
public:
    using QAbstractTableModel::QAbstractTableModel;
    void setRows(std::vector<humtrace::ui::FindingRow> rows);
    [[nodiscard]] const humtrace::ui::FindingRow& row(int index) const { return rows_[static_cast<std::size_t>(index)]; }
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<humtrace::ui::FindingRow> rows_;
};
