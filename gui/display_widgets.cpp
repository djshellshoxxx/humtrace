// Purpose: implementation of WaveformWidget and SpectrogramWidget.
#include "display_widgets.hpp"

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
const QColor kTrack(0xff, 0xff, 0xff);
const QColor kTrackMains(0xff, 0x7a, 0x59);

const humtrace::ChannelDisplay* displayFor(const humtrace::AnalysisReport* r, int channelIndex) {
    if (!r || channelIndex < 0 || channelIndex >= static_cast<int>(r->channel_results.size())) return nullptr;
    const auto number = r->channel_results[static_cast<std::size_t>(channelIndex)].channel_number;
    for (const auto& d : r->display) if (d.channel_number == number) return &d;
    return nullptr;
}

double hopSeconds(const humtrace::AnalysisReport& r) {
    return r.sample_rate_hz ? static_cast<double>(r.hop_size_samples) / r.sample_rate_hz : 0.0;
}

QString freqLabel(double hz) {
    return hz >= 1000.0 ? QStringLiteral("%1k").arg(hz / 1000.0, 0, 'g', 3) : QString::number(hz, 'g', 3);
}
}

QRgb perceptualColor(double t) {
    static const double stops[][3] = {
        {68, 1, 84}, {72, 40, 120}, {62, 74, 137}, {49, 104, 142}, {38, 130, 142},
        {31, 158, 137}, {53, 183, 121}, {109, 205, 89}, {180, 222, 44}, {253, 231, 37}};
    constexpr int n = static_cast<int>(sizeof stops / sizeof stops[0]);
    t = std::isfinite(t) ? std::clamp(t, 0.0, 1.0) : 0.0;
    const double x = t * (n - 1);
    const int i = std::min(static_cast<int>(x), n - 2);
    const double f = x - i;
    auto ch = [&](int c) { return static_cast<int>(std::lround(stops[i][c] + (stops[i + 1][c] - stops[i][c]) * f)); };
    return qRgb(ch(0), ch(1), ch(2));
}

// ---- WaveformWidget ----

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(140);
    setAccessibleName(tr("Waveform"));
    setAccessibleDescription(tr("Minimum and maximum sample amplitude over time for the selected channel. "
                                "The marker shows the selected analysis frame."));
}

const humtrace::WaveformEnvelope* WaveformWidget::envelope() const {
    const auto* d = displayFor(report_, channel_);
    return d && !d->waveform.minimum.empty() ? &d->waveform : nullptr;
}

void WaveformWidget::setReport(const humtrace::AnalysisReport* report, int channelIndex) {
    report_ = report; channel_ = channelIndex; selected_ = -1; update();
}
void WaveformWidget::setSelectedFrame(int frameIndex) { selected_ = frameIndex; update(); }

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), kBackground);
    const QRect plot = rect().adjusted(46, 10, -10, -26);
    p.setPen(kGrid);
    for (int g = -1; g <= 1; ++g) {
        const int y = plot.center().y() - static_cast<int>(g * plot.height() / 2.0);
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(kText); p.drawText(2, y + 4, g == 0 ? QStringLiteral("0") : QStringLiteral("%1").arg(g > 0 ? "+1" : "-1")); p.setPen(kGrid);
    }
    p.setPen(kText);
    const auto* env = envelope();
    if (!env) { p.drawText(plot, Qt::AlignCenter, tr("No waveform loaded")); return; }
    const double duration = report_->duration_seconds;
    for (int t = 0; t <= 4; ++t) {
        const double sec = duration * t / 4.0;
        const int x = plot.left() + plot.width() * t / 4;
        p.setPen(kGrid); p.drawLine(x, plot.top(), x, plot.bottom());
        p.setPen(kText);
        p.drawText(std::clamp(x - 14, 0, std::max(0, width() - 40)), height() - 8, QStringLiteral("%1 s").arg(sec, 0, 'f', 2));
    }
    const std::size_t n = env->minimum.size();
    const double step = static_cast<double>(plot.width()) / static_cast<double>(n);
    const double mid = plot.center().y(), half = plot.height() / 2.0;
    p.setPen(QPen(kAccent, 1.0));
    for (std::size_t i = 0; i < n; ++i) {
        const double x = plot.left() + static_cast<double>(i) * step;
        p.drawLine(QPointF(x, mid - env->maximum[i] * half), QPointF(x, mid - env->minimum[i] * half - 0.5));
    }
    if (selected_ >= 0 && report_->sample_rate_hz && duration > 0) {
        const double t0 = static_cast<double>(selected_) * hopSeconds(*report_);
        const double t1 = t0 + static_cast<double>(report_->frame_size_samples) / report_->sample_rate_hz;
        const double x0 = plot.left() + plot.width() * std::min(1.0, t0 / duration);
        const double x1 = plot.left() + plot.width() * std::min(1.0, t1 / duration);
        p.fillRect(QRectF(x0, plot.top(), std::max(2.0, x1 - x0), plot.height()), QColor(0xff, 0xb3, 0x47, 60));
        p.setPen(QPen(QColor(0xff, 0xb3, 0x47), 2));
        p.drawLine(QPointF(x0, plot.top()), QPointF(x0, plot.bottom()));
    }
}

// ---- SpectrogramWidget ----

SpectrogramWidget::SpectrogramWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(160);
    setAccessibleName(tr("Spectrogram"));
    setAccessibleDescription(tr("Level in dBFS by log frequency (vertical) and time (horizontal). "
                                "Lines mark persistent tone tracks. Click or use arrow keys to select a frame."));
}

const humtrace::Spectrogram* SpectrogramWidget::spectrogram() const {
    const auto* d = displayFor(report_, channel_);
    return d && d->spectrogram.rows > 1 && d->spectrogram.columns() > 0 &&
           d->spectrogram.row_edges_hz.size() == d->spectrogram.rows + 1 ? &d->spectrogram : nullptr;
}

int SpectrogramWidget::columnCount() const {
    const auto* s = spectrogram();
    return s ? static_cast<int>(s->columns()) : 0;
}

int SpectrogramWidget::frameCount() const {
    if (!report_ || channel_ < 0 || channel_ >= static_cast<int>(report_->channel_results.size())) return 0;
    return static_cast<int>(report_->channel_results[static_cast<std::size_t>(channel_)].frames.size());
}

QRect SpectrogramWidget::plotRect() const { return rect().adjusted(50, 10, -70, -26); }

void SpectrogramWidget::setReport(const humtrace::AnalysisReport* report, int channelIndex) {
    report_ = report; channel_ = channelIndex; selected_ = -1;
    rebuildImage();
    update();
}

void SpectrogramWidget::setSelectedFrame(int frameIndex) { selected_ = frameIndex; update(); }

void SpectrogramWidget::rebuildImage() {
    image_ = QImage();
    const auto* s = spectrogram();
    if (!s) return;
    const int cols = static_cast<int>(s->columns()), rows = static_cast<int>(s->rows);
    image_ = QImage(cols, rows, QImage::Format_RGB32);
    constexpr double span = kCeilDbfs - kFloorDbfs;
    for (int r = 0; r < rows; ++r) {
        auto* line = reinterpret_cast<QRgb*>(image_.scanLine(rows - 1 - r));   // low frequency at bottom
        for (int c = 0; c < cols; ++c) {
            const double db = s->at(static_cast<std::size_t>(c), static_cast<std::size_t>(r));
            line[c] = perceptualColor(std::isfinite(db) ? (db - kFloorDbfs) / span : 0.0);
        }
    }
}

double SpectrogramWidget::rowPosition(double f) const {
    const auto& e = spectrogram()->row_edges_hz;
    if (f <= e.front()) return 0.0;
    if (f >= e.back()) return static_cast<double>(e.size() - 1);
    const auto it = std::upper_bound(e.begin(), e.end(), f);
    const std::size_t r = static_cast<std::size_t>(it - e.begin()) - 1;
    return static_cast<double>(r) + (f - e[r]) / (e[r + 1] - e[r]);
}

void SpectrogramWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), kBackground);
    const QRect plot = plotRect();
    p.setPen(kText);
    const auto* s = spectrogram();
    if (!s || image_.isNull()) { p.drawText(plot, Qt::AlignCenter, tr("No spectrogram loaded")); return; }
    p.drawImage(plot, image_);
    const double rows = static_cast<double>(s->rows);
    auto yOf = [&](double hz) { return plot.bottom() - plot.height() * rowPosition(hz) / rows; };
    const double duration = report_->duration_seconds;
    auto xOfSec = [&](double sec) { return plot.left() + plot.width() * std::clamp(sec / std::max(duration, 1e-9), 0.0, 1.0); };
    const double rate = static_cast<double>(report_->sample_rate_hz);

    // Frequency axis (log): decade-ish ticks inside the displayed range.
    static const double ticks[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000};
    for (double hz : ticks) {
        if (hz < s->row_edges_hz.front() || hz > s->row_edges_hz.back()) continue;
        const double y = yOf(hz);
        p.setPen(QColor(255, 255, 255, 50)); p.drawLine(plot.left(), static_cast<int>(y), plot.right(), static_cast<int>(y));
        p.setPen(kText); p.drawText(QRectF(0, y - 8, plot.left() - 4, 16), Qt::AlignRight | Qt::AlignVCenter, freqLabel(hz));
    }
    p.setPen(kText);
    p.drawText(2, plot.top() - 1, tr("Hz"));
    for (int t = 0; t <= 4; ++t) {
        const double sec = duration * t / 4.0;
        const int x = plot.left() + plot.width() * t / 4;
        p.drawText(std::clamp(x - 14, 0, std::max(0, width() - 40)), height() - 8, QStringLiteral("%1 s").arg(sec, 0, 'f', 2));
    }

    // Tone tracks: time span at the median frequency.
    const auto& tracks = report_->channel_results[static_cast<std::size_t>(channel_)].tracks;
    for (const auto& t : tracks) {
        if (rate <= 0) break;
        const double y = yOf(t.median_frequency_hz);
        p.setPen(QPen(t.mains.nominal_hz > 0 ? kTrackMains : kTrack, 2));
        p.drawLine(QPointF(xOfSec(static_cast<double>(t.start_sample) / rate), y),
                   QPointF(xOfSec(static_cast<double>(t.end_sample) / rate), y));
    }

    if (selected_ >= 0 && rate > 0) {
        const double x = xOfSec(static_cast<double>(selected_) * hopSeconds(*report_));
        p.setPen(QPen(kAccent, 2)); p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }

    // Colorbar legend.
    const QRect bar(plot.right() + 14, plot.top(), 12, plot.height());
    for (int y = 0; y < bar.height(); ++y)
        p.fillRect(bar.left(), bar.bottom() - y, bar.width(), 1,
                   QColor(perceptualColor(static_cast<double>(y) / std::max(1, bar.height() - 1))));
    p.setPen(kText);
    p.drawText(bar.right() + 4, bar.top() + 10, QStringLiteral("0"));
    p.drawText(bar.right() + 4, bar.center().y() + 4, QString::number(static_cast<int>((kFloorDbfs + kCeilDbfs) / 2)));
    p.drawText(bar.right() + 4, bar.bottom(), QString::number(static_cast<int>(kFloorDbfs)));
    p.drawText(bar.left() - 2, bar.top() - 1, tr("dBFS"));
    if (hasFocus()) { p.setPen(QPen(kText, 1, Qt::DashLine)); p.drawRect(rect().adjusted(0, 0, -1, -1)); }
}

void SpectrogramWidget::mousePressEvent(QMouseEvent* e) {
    setFocus();
    const auto* s = spectrogram();
    const int n = frameCount();
    if (!s || n == 0) return;
    const QRect plot = plotRect();
    const double x = e->position().x() - plot.left();
    if (x < 0 || x >= plot.width()) return;
    const std::size_t cols = s->columns();
    const std::size_t column = std::min(cols - 1, static_cast<std::size_t>(x * static_cast<double>(cols) / plot.width()));
    const int frame = std::min(n - 1, static_cast<int>(column * s->frames_per_column));
    emit frameClicked(frame);
}

void SpectrogramWidget::keyPressEvent(QKeyEvent* e) {
    const int n = frameCount();
    if (n == 0) { QWidget::keyPressEvent(e); return; }
    const auto* s = spectrogram();
    const int stride = s ? static_cast<int>(std::max<std::size_t>(1, s->frames_per_column)) : 1;
    int idx = selected_;
    switch (e->key()) {
    case Qt::Key_Left: idx = std::max(0, idx - stride); break;
    case Qt::Key_Right: idx = std::min(n - 1, idx + stride); break;
    case Qt::Key_Home: idx = 0; break;
    case Qt::Key_End: idx = n - 1; break;
    default: QWidget::keyPressEvent(e); return;
    }
    if (idx != selected_) emit frameClicked(idx);
}
