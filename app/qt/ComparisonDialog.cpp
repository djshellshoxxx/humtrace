#include "ComparisonDialog.hpp"

#include "humtrace/session.hpp"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

ComparisonDialog::ComparisonDialog(const humtrace::ComparisonReport& report, QWidget* parent)
    : QDialog(parent), report_(report) {
    setWindowTitle(tr("Recording comparison"));
    resize(820, 520);
    auto* layout = new QVBoxLayout(this);
    // Persistent banner: alignment/resampling state is always visible.
    auto* banner = new QLabel(tr("Independent summary. No time alignment applied. No resampling applied. "
                                 "Channels compared by identical number only."));
    banner->setWordWrap(true);
    banner->setAccessibleName(tr("Comparison method"));
    layout->addWidget(banner);
    auto* files = new QLabel(tr("A: %1 (%2 Hz, %3 s)    B: %4 (%5 Hz, %6 s)")
        .arg(fontMetrics().elidedText(QFileInfo(QString::fromStdString(report.a.path)).fileName(), Qt::ElideMiddle, 300), QString::number(report.a.sample_rate_hz),
             QString::number(report.a.duration_seconds, 'f', 2),
             fontMetrics().elidedText(QFileInfo(QString::fromStdString(report.b.path)).fileName(), Qt::ElideMiddle, 300), QString::number(report.b.sample_rate_hz),
             QString::number(report.b.duration_seconds, 'f', 2)));
    files->setTextFormat(Qt::PlainText);
    files->setWordWrap(true);
    files->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(files);
    for (const auto& w : report.warnings) {
        auto* label = new QLabel(tr("Warning: ") + QString::fromStdString(w));
        label->setTextFormat(Qt::PlainText);
        layout->addWidget(label);
    }

    table_ = new QTableWidget(0, 7);
    table_->setHorizontalHeaderLabels({tr("Status"), tr("Channel A"), tr("Freq A (Hz)"), tr("Channel B"), tr("Freq B (Hz)"), tr("Delta freq (Hz)"), tr("Delta level (dB)")});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAccessibleName(tr("Comparison table"));
    table_->setAccessibleDescription(tr("Measured track summaries. Unmatched frequencies and level differences are unavailable. No alignment or resampling is applied."));
    page_ = new QLabel;
    page_->setAccessibleName(tr("Comparison page and total count"));
    previous_ = new QPushButton(tr("Previous rows")); next_ = new QPushButton(tr("Next rows"));
    connect(previous_, &QPushButton::clicked, this, [this] { if (first_ >= 256) setPage(first_ - 256); });
    connect(next_, &QPushButton::clicked, this, [this] { setPage(first_ + 256); });
    auto* pages = new QHBoxLayout; pages->addWidget(previous_); pages->addWidget(page_); pages->addWidget(next_);
    layout->addLayout(pages); layout->addWidget(table_, 1);
    setPage(0);
    layout->addWidget(new QLabel(tr("Similar frequencies do not show a common device, environment, or location.")));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto* save = buttons->addButton(tr("Export comparison JSON"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(this, tr("Export comparison"), "comparison.json", tr("JSON (*.json)"));
        if (path.isEmpty()) return;
        try {
            humtrace::require_distinct_paths(path.toStdString(), report_.a.path);
            humtrace::require_distinct_paths(path.toStdString(), report_.b.path);
            humtrace::write_file_atomically(path.toStdString(), humtrace::serialize_comparison_json(report_));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("HumTrace"), tr("Export failed: %1").arg(QString::fromUtf8(e.what())));
        }
    });
}

void ComparisonDialog::setPage(std::size_t first) {
    const auto total = report_.matches.size() + report_.unmatched_a.size() + report_.unmatched_b.size();
    first_ = std::min(first, total ? ((total - 1) / 256) * 256 : 0);
    const auto end = std::min(total, first_ + 256);
    table_->setRowCount(0);
    auto add = [&](const QStringList& cells) {
        const int row = table_->rowCount(); table_->insertRow(row);
        for (int c = 0; c < 7; ++c) {
            auto* item = new QTableWidgetItem(cells[c]);
            item->setData(Qt::AccessibleTextRole, tr("%1: %2; %3")
                .arg(table_->horizontalHeaderItem(c)->text(), cells[c] == "-" ? tr("unavailable") : cells[c], cells[0]));
            table_->setItem(row, c, item);
        }
    };
    for (auto i = first_; i < end; ++i) {
        if (i < report_.matches.size()) {
            const auto& m = report_.matches[i];
            add({tr("Matched"), QString::number(m.a.channel), QString::number(m.a.median_frequency_hz, 'f', 3), QString::number(m.b.channel),
                QString::number(m.b.median_frequency_hz, 'f', 3), QString::number(m.delta_frequency_hz, 'f', 3), QString::number(m.delta_level_db, 'f', 2)});
        } else if (i - report_.matches.size() < report_.unmatched_a.size()) {
            const auto& u = report_.unmatched_a[i - report_.matches.size()];
            add({tr("Only in A"), QString::number(u.channel), QString::number(u.median_frequency_hz, 'f', 3), "-", "-", "-", "-"});
        } else {
            const auto& u = report_.unmatched_b[i - report_.matches.size() - report_.unmatched_a.size()];
            add({tr("Only in B"), "-", "-", QString::number(u.channel), QString::number(u.median_frequency_hz, 'f', 3), "-", "-"});
        }
    }
    table_->resizeColumnsToContents();
    page_->setText(tr("Rows %1–%2 of %3").arg(total ? first_ + 1 : 0).arg(end).arg(total));
    previous_->setEnabled(first_ > 0); next_->setEnabled(end < total);
}
