#include "timeline_widget.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace {
const QColor kBackground(0x14, 0x1a, 0x24);
const QColor kGrid(0x2c, 0x36, 0x46);
const QColor kText(0xe6, 0xec, 0xf5);
const QColor kAccent(0x4d, 0xa3, 0xff);
const QColor kSupport50(0xff, 0xb3, 0x47);
const QColor kSupport60(0x6b, 0xd6, 0x9a);
}

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(140);
    setAccessibleName(tr("Finding timeline"));
    setAccessibleDescription(tr("Per-frame count of supported 50 Hz and 60 Hz harmonics. "
                                "The findings table lists the same values."));
}

int TimelineWidget::frameCount() const {
    if (!report_ || channel_ < 0 || channel_ >= static_cast<int>(report_->channel_results.size())) return 0;
    return static_cast<int>(report_->channel_results[static_cast<std::size_t>(channel_)].frames.size());
}

void TimelineWidget::setReport(const humtrace::AnalysisReport* report, int channelIndex) {
    report_ = report; channel_ = channelIndex; selected_ = -1; update();
}

void TimelineWidget::setSelectedFrame(int frameIndex) { selected_ = frameIndex; update(); }

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), kBackground);
    const int n = frameCount();
    const QRect plot = rect().adjusted(40, 10, -10, -22);
    p.setPen(kGrid);
    for (int g = 0; g <= 10; g += 2) {
        const int y = plot.bottom() - plot.height() * g / 10;
        p.drawLine(plot.left(), y, plot.right(), y);
    }
    p.setPen(kText);
    p.drawText(2, plot.top() + 10, tr("10"));
    p.drawText(2, plot.bottom(), tr("0"));
    p.drawText(plot.left(), height() - 6, tr("Supported harmonics per frame (orange 50 Hz, green 60 Hz); time →"));
    if (n == 0) { p.drawText(plot, Qt::AlignCenter, tr("No analysis loaded")); return; }
    const auto& frames = report_->channel_results[static_cast<std::size_t>(channel_)].frames;
    const double step = static_cast<double>(plot.width()) / n;
    const double barWidth = std::max(1.0, step / 2.0);
    for (int i = 0; i < n; ++i) {
        const double x = plot.left() + i * step;
        const auto& f = frames[static_cast<std::size_t>(i)];
        const double h50 = plot.height() * std::min<std::size_t>(f.mains_50.supporting_harmonics, 10) / 10.0;
        const double h60 = plot.height() * std::min<std::size_t>(f.mains_60.supporting_harmonics, 10) / 10.0;
        p.fillRect(QRectF(x, plot.bottom() - h50, barWidth, h50), kSupport50);
        p.fillRect(QRectF(x + barWidth, plot.bottom() - h60, barWidth, h60), kSupport60);
    }
    if (selected_ >= 0 && selected_ < n) {
        p.setPen(QPen(kAccent, 2));
        p.drawRect(QRectF(plot.left() + selected_ * step, plot.top(), step, plot.height()));
    }
    if (hasFocus()) { p.setPen(QPen(kText, 1, Qt::DashLine)); p.drawRect(rect().adjusted(0, 0, -1, -1)); }
}

void TimelineWidget::mousePressEvent(QMouseEvent* e) {
    setFocus();
    const int n = frameCount();
    if (n == 0) return;
    const QRect plot = rect().adjusted(40, 10, -10, -22);
    const int idx = static_cast<int>((e->position().x() - plot.left()) * n / std::max(1, plot.width()));
    if (idx >= 0 && idx < n) emit frameClicked(idx);
}

void TimelineWidget::keyPressEvent(QKeyEvent* e) {
    const int n = frameCount();
    if (n == 0) { QWidget::keyPressEvent(e); return; }
    int idx = selected_;
    switch (e->key()) {
    case Qt::Key_Left: idx = std::max(0, idx - 1); break;
    case Qt::Key_Right: idx = std::min(n - 1, idx + 1); break;
    case Qt::Key_Home: idx = 0; break;
    case Qt::Key_End: idx = n - 1; break;
    default: QWidget::keyPressEvent(e); return;
    }
    if (idx != selected_) emit frameClicked(idx);
}

SpectrumWidget::SpectrumWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(140);
    setAccessibleName(tr("Frame spectrum"));
    setAccessibleDescription(tr("Spectrum of the selected frame in dBFS; strongest peak is in the table."));
}

void SpectrumWidget::setSpectrum(std::vector<humtrace::SpectrumBin> bins) { bins_ = std::move(bins); update(); }

void SpectrumWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), kBackground);
    const QRect plot = rect().adjusted(46, 10, -10, -22);
    p.setPen(kGrid);
    constexpr double floorDb = -120.0;
    for (int db = 0; db >= -120; db -= 20) {
        const int y = plot.top() + static_cast<int>(plot.height() * (-db / 120.0));
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(kText); p.drawText(2, y + 4, QString::number(db)); p.setPen(kGrid);
    }
    p.setPen(kText);
    if (bins_.size() < 2) { p.drawText(plot, Qt::AlignCenter, tr("Select a frame")); return; }
    p.drawText(plot.left(), height() - 6, tr("0 Hz → %1 Hz (dBFS)").arg(bins_.back().frequency_hz, 0, 'f', 0));
    QPolygonF line;
    const double n = static_cast<double>(bins_.size() - 1);
    for (std::size_t i = 0; i < bins_.size(); ++i) {
        const double db = std::isfinite(bins_[i].level_dbfs) ? std::max(floorDb, std::min(0.0, bins_[i].level_dbfs)) : floorDb;
        line << QPointF(plot.left() + plot.width() * static_cast<double>(i) / n,
                        plot.top() + plot.height() * (-db / 120.0));
    }
    p.setPen(QPen(kAccent, 1.2));
    p.drawPolyline(line);
}
