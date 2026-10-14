#pragma once

#include "humtrace/compare.hpp"

#include <QDialog>

class ComparisonDialog : public QDialog {
    Q_OBJECT
public:
    ComparisonDialog(const humtrace::ComparisonReport& report, QWidget* parent = nullptr);

private:
    humtrace::ComparisonReport report_;
};
