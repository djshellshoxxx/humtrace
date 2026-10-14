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
        .arg(QFileInfo(QString::fromStdString(report.a.path)).fileName(), QString::number(report.a.sample_rate_hz),
             QString::number(report.a.duration_seconds, 'f', 2),
             QFileInfo(QString::fromStdString(report.b.path)).fileName(), QString::number(report.b.sample_rate_hz),
             QString::number(report.b.duration_seconds, 'f', 2)));
    files->setTextFormat(Qt::PlainText);
    files->setWordWrap(true);
    layout->addWidget(files);
    for (const auto& w : report.warnings) {
        auto* label = new QLabel(tr("Warning: ") + QString::fromStdString(w));
        label->setTextFormat(Qt::PlainText);
        layout->addWidget(label);
    }

    auto* table = new QTableWidget(0, 7);
    table->setHorizontalHeaderLabels({tr("Status"), tr("Channel A"), tr("Freq A (Hz)"), tr("Channel B"), tr("Freq B (Hz)"), tr("Delta freq (Hz)"), tr("Delta level (dB)")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAccessibleName(tr("Comparison table"));
    auto add = [&](const QString& status, const QString& ca, const QString& fa, const QString& cb, const QString& fb, const QString& df, const QString& dl) {
        const int r = table->rowCount();
        table->insertRow(r);
        const QString cells[] = {status, ca, fa, cb, fb, df, dl};
        for (int c = 0; c < 7; ++c) table->setItem(r, c, new QTableWidgetItem(cells[c]));
    };
    for (const auto& m : report.matches)
        add(tr("Matched"), QString::number(m.a.channel), QString::number(m.a.median_frequency_hz, 'f', 3), QString::number(m.b.channel),
            QString::number(m.b.median_frequency_hz, 'f', 3), QString::number(m.delta_frequency_hz, 'f', 3), QString::number(m.delta_level_db, 'f', 2));
    for (const auto& u : report.unmatched_a)
        add(tr("Only in A"), QString::number(u.channel), QString::number(u.median_frequency_hz, 'f', 3), "-", "-", "-", "-");
    for (const auto& u : report.unmatched_b)
        add(tr("Only in B"), "-", "-", QString::number(u.channel), QString::number(u.median_frequency_hz, 'f', 3), "-", "-");
    table->resizeColumnsToContents();
    layout->addWidget(table, 1);
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
