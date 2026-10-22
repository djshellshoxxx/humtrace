#pragma once

#include "humtrace/compare.hpp"

#include <QDialog>

class QTableWidget;
class QLabel;
class QPushButton;

class ComparisonDialog : public QDialog {
    Q_OBJECT
public:
    ComparisonDialog(const humtrace::ComparisonReport& report, QWidget* parent = nullptr);

private:
    void setPage(std::size_t first);
    humtrace::ComparisonReport report_;
    QTableWidget* table_{nullptr};
    QLabel* page_{nullptr};
    QPushButton* previous_{nullptr};
    QPushButton* next_{nullptr};
    std::size_t first_{0};
};
