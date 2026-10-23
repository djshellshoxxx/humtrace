#include "SpectrumChart.hpp"

#include "../presentation/plot_math.hpp"
#include "Theme.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

SpectrumChart::SpectrumChart(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumHeight(110);
    setAccessibleName(tr("Spectrum of the frame at the cursor"));
}

void SpectrumChart::setFrame(const std::vector<float>* levels, const std::vector<double>* rowHz, double nyquistHz,
                             double floorDb, double ceilingDb) {
    levels_ = levels;
    rowHz_ = rowHz;
    nyquist_ = nyquistHz;
    floor_ = floorDb;
    ceiling_ = std::max(ceilingDb, floorDb + 1.0); // never an empty or inverted range
    hoverRow_ = -1;
    readout_.clear();
    setAccessibleDescription(levels_ && !levels_->empty()
        ? tr("Use Left and Right to read display rows; the Spectrum table provides the values and units. Frequency uncertainty is not estimated.")
        : tr("No frame selected"));
    update();
}

void SpectrumChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const auto t = theme::current();
    p.fillRect(rect(), t.surface);
    const QRect r(56, 6, std::max(10, width() - 64), std::max(10, height() - 28));
    p.setPen(QPen(hasFocus() ? t.focus : t.raised, hasFocus() ? 3 : 1));
    p.drawRect(r);
    if (!levels_ || !rowHz_ || levels_->empty()) {
        p.setPen(t.textSecondary);
        p.drawText(r, Qt::AlignCenter, tr("No frame selected"));
        return;
    }
    const double minHz = rowHz_->front();
    auto xAtHz = [&](double hz) { return r.left() + humtrace::ui::log_position(hz, minHz, nyquist_) * r.width(); };
    auto yAtDb = [&](double db) { return humtrace::ui::map_linear(db, floor_, ceiling_, r.bottom(), r.top()); };
    p.setPen(t.textSecondary);
    for (double db = std::ceil(floor_ / 20.0) * 20.0; db <= ceiling_; db += 20.0) {
        p.drawText(QRect(0, static_cast<int>(yAtDb(db)) - 8, 52, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(db, 'f', 0) + " dB");
        p.setPen(QPen(t.raised, 1));
        p.drawLine(r.left(), static_cast<int>(yAtDb(db)), r.right(), static_cast<int>(yAtDb(db)));
        p.setPen(t.textSecondary);
    }
    for (const double hz : {20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0})
        if (hz >= minHz && hz <= nyquist_)
            p.drawText(QRect(static_cast<int>(xAtHz(hz)) - 30, r.bottom() + 3, 60, 18), Qt::AlignHCenter,
                       hz >= 1000 ? QString::number(hz / 1000.0) + "k" : QString::number(hz));
    // Nominal reference lines (context only, not a detection).
    p.setPen(QPen(t.compare, 1, Qt::DashLine));
    for (const double hz : {50.0, 60.0})
        if (hz >= minHz && hz <= nyquist_) p.drawLine(static_cast<int>(xAtHz(hz)), r.top(), static_cast<int>(xAtHz(hz)), r.bottom());
    QPolygonF line;
    for (std::size_t i = 0; i < levels_->size(); ++i) {
        const double center = std::sqrt((*rowHz_)[i] * (i + 1 < rowHz_->size() ? (*rowHz_)[i + 1] : nyquist_));
        line << QPointF(xAtHz(center), yAtDb(std::clamp<double>((*levels_)[i], floor_, ceiling_)));
    }
    p.setPen(QPen(t.signal, 2));
    p.drawPolyline(line);
    if (hoverRow_ >= 0 && hoverRow_ < static_cast<int>(line.size())) {
        p.setPen(QPen(t.event, 2));
        p.drawEllipse(line[hoverRow_], 4, 4);
    }
    p.setPen(t.text);
    p.drawText(r.adjusted(6, 2, -6, 0), Qt::AlignLeft | Qt::AlignTop, readout_.isEmpty() ? tr("Hover or use Left/Right for a readout") : readout_);
    p.setPen(t.textSecondary);
    p.drawText(r.adjusted(6, 2, -6, 0), Qt::AlignRight | Qt::AlignTop, tr("dashed: 50 / 60 Hz reference"));
}

void SpectrumChart::setHoverRow(int row) {
    if (!levels_ || !rowHz_ || levels_->empty()) return;
    hoverRow_ = std::clamp(row, 0, static_cast<int>(levels_->size()) - 1);
    const auto i = static_cast<std::size_t>(hoverRow_);
    const double hi = i + 1 < rowHz_->size() ? (*rowHz_)[i + 1] : nyquist_;
    readout_ = tr("%1 to %2 Hz: %3 dBFS (maximum FFT bin, floor-clamped; display row width %4 Hz; uncertainty is not estimated)")
                   .arg((*rowHz_)[i], 0, 'f', 1).arg(hi, 0, 'f', 1).arg((*levels_)[i], 0, 'f', 1)
                   .arg(hi - (*rowHz_)[i], 0, 'f', 1);
    setAccessibleDescription(readout_);
    emit readoutChanged(readout_);
    update();
}

void SpectrumChart::mouseMoveEvent(QMouseEvent* e) {
    if (!levels_ || !rowHz_ || levels_->empty()) return;
    const QRect r(56, 6, std::max(10, width() - 64), std::max(10, height() - 28));
    const double pos = (e->position().x() - r.left()) / r.width();
    const double hz = humtrace::ui::hz_from_log_position(pos, rowHz_->front(), nyquist_);
    const auto it = std::upper_bound(rowHz_->begin(), rowHz_->end(), hz);
    setHoverRow(static_cast<int>(it - rowHz_->begin()) - 1);
}

void SpectrumChart::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Left) setHoverRow(hoverRow_ < 0 ? 0 : hoverRow_ - 1);
    else if (e->key() == Qt::Key_Right) setHoverRow(hoverRow_ + 1);
    else QWidget::keyPressEvent(e);
}
