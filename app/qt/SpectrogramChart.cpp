#include "SpectrogramChart.hpp"

#include "Theme.hpp"

#include <QPainter>
#include <algorithm>

SpectrogramChart::SpectrogramChart(ViewState* state, QWidget* parent)
    : TimeChart(state, tr("Spectrogram"), parent) {
    connect(state, &ViewState::channelChanged, this, [this] { rebuildImage(); });
}

void SpectrogramChart::setData(const std::vector<humtrace::ChannelView>* views, const humtrace::AnalysisReport* report,
                               const std::vector<humtrace::ui::FindingRow>* findings) {
    views_ = views;
    report_ = report;
    findings_ = findings;
    selectedFinding_ = -1;
    rebuildImage();
}

void SpectrogramChart::setDisplay(bool logFrequency, humtrace::ui::Palette palette, double floorDb, double ceilingDb) {
    log_ = logFrequency;
    palette_ = palette;
    floor_ = floorDb;
    ceiling_ = std::max(ceilingDb, floorDb + 1.0);
    rebuildImage();
}

void SpectrogramChart::rebuildImage() {
    image_ = QImage();
    const int channel = state_->channel();
    if (!views_ || !report_ || channel < 0 || channel >= static_cast<int>(views_->size())) { update(); return; }
    const auto& v = (*views_)[static_cast<std::size_t>(channel)];
    if (v.spectrogram_dbfs.empty() || v.row_frequency_hz.empty()) { update(); return; }
    const int columns = static_cast<int>(v.spectrogram_dbfs.size());
    const int rows = static_cast<int>(v.row_frequency_hz.size());
    const double nyquist = report_->sample_rate_hz / 2.0;
    const double minHz = v.row_frequency_hz.front();
    const int height = log_ ? rows : 256;
    image_ = QImage(columns, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        // Top of the image is the highest frequency.
        int row = rows - 1 - y;
        if (!log_) {
            const double hz = nyquist * (1.0 - (y + 0.5) / height);
            const auto it = std::upper_bound(v.row_frequency_hz.begin(), v.row_frequency_hz.end(), hz);
            row = std::max(0, static_cast<int>(it - v.row_frequency_hz.begin()) - 1);
        }
        (void)minHz;
        auto* line = reinterpret_cast<QRgb*>(image_.scanLine(y));
        for (int x = 0; x < columns; ++x) {
            const double level = v.spectrogram_dbfs[static_cast<std::size_t>(x)][static_cast<std::size_t>(row)];
            const auto c = humtrace::ui::colormap(humtrace::ui::normalize_db(level, floor_, ceiling_), palette_);
            line[x] = qRgb(c.r, c.g, c.b);
        }
    }
    update();
}

QString SpectrogramChart::columnDescription(int frame) const {
    return tr("Frame %1").arg(frame);
}

void SpectrogramChart::paintData(QPainter& p, const QRect& plot) {
    if (image_.isNull() || !report_) {
        p.setPen(theme::current().textSecondary);
        p.drawText(plot, Qt::AlignCenter, tr("No spectrogram"));
        return;
    }
    const auto t = theme::current();
    const double rate = report_->sample_rate_hz;
    const double hopSec = static_cast<double>(report_->hop_size_samples) / rate;
    const double windowSec = static_cast<double>(report_->frame_size_samples) / rate;
    const auto view = state_->view();
    // Draw the visible column range scaled to the plot; columns are windows starting at i * hop.
    const double x0 = xAt(0.0), x1 = xAt(hopSec * (image_.width() - 1) + windowSec);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(QRectF(x0, plot.top(), x1 - x0, plot.height()), image_);
    (void)view;

    const double nyquist = rate / 2.0;
    const auto& rowsHz = (*views_)[static_cast<std::size_t>(state_->channel())].row_frequency_hz;
    const double minHz = rowsHz.front();
    auto yAtHz = [&](double hz) {
        const double pos = log_ ? humtrace::ui::log_position(hz, minHz, nyquist) : std::clamp(hz / nyquist, 0.0, 1.0);
        return plot.bottom() - pos * plot.height();
    };
    // Overlay measured tracks of the selected channel: dashed line at median frequency.
    if (findings_) {
        for (int i = 0; i < static_cast<int>(findings_->size()); ++i) {
            const auto& f = (*findings_)[static_cast<std::size_t>(i)];
            if (f.channel != state_->channel() + 1) continue;
            const bool sel = i == selectedFinding_;
            p.setPen(QPen(sel ? t.signal : t.event, sel ? 3 : 1, sel ? Qt::SolidLine : Qt::DashLine));
            const int y = static_cast<int>(yAtHz(f.median_frequency_hz));
            p.drawLine(static_cast<int>(xAt(f.start_seconds)), y, static_cast<int>(xAt(f.end_seconds)), y);
        }
    }
    p.setPen(t.textSecondary);
    p.drawText(plot.adjusted(4, 2, -4, 0), Qt::AlignRight | Qt::AlignTop,
               tr("%1 frequency, %2 to %3 dBFS peak amplitude per row")
                   .arg(log_ ? tr("log") : tr("linear")).arg(floor_, 0, 'f', 0).arg(ceiling_, 0, 'f', 0));
}

void SpectrogramChart::paintAxes(QPainter& p, const QRect& plot) {
    if (image_.isNull() || !report_ || !views_) return;
    const auto t = theme::current();
    const double nyquist = report_->sample_rate_hz / 2.0;
    const double minHz = (*views_)[static_cast<std::size_t>(state_->channel())].row_frequency_hz.front();
    auto yAtHz = [&](double hz) {
        const double pos = log_ ? humtrace::ui::log_position(hz, minHz, nyquist) : std::clamp(hz / nyquist, 0.0, 1.0);
        return plot.bottom() - pos * plot.height();
    };
    p.setPen(t.text);
    const double ticks[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    const double linTicks[] = {0, 0.25, 0.5, 0.75, 1.0};
    if (log_) {
        for (const double hz : ticks)
            if (hz >= minHz && hz <= nyquist) {
                const int y = std::clamp(static_cast<int>(yAtHz(hz)), plot.top() + 8, plot.bottom() - 8);
                p.drawText(QRect(0, y - 8, kLeft - 4, 16), Qt::AlignRight | Qt::AlignVCenter,
                           hz >= 1000 ? QString::number(hz / 1000.0) + " kHz" : QString::number(hz) + " Hz");
            }
    } else {
        for (const double f : linTicks) {
            const int y = std::clamp(static_cast<int>(yAtHz(f * nyquist)), plot.top() + 8, plot.bottom() - 8);
            p.drawText(QRect(0, y - 8, kLeft - 4, 16), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(f * nyquist / 1000.0, 'f', 1) + " kHz");
        }
    }
}
