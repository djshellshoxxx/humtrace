#include "SpectrogramChart.hpp"

#include "Theme.hpp"

#include <QPainter>
#include <QResizeEvent>
#include <algorithm>

SpectrogramChart::SpectrogramChart(ViewState* state, QWidget* parent)
    : TimeChart(state, tr("Spectrogram"), parent) {
    connect(state, &ViewState::channelChanged, this, [this] { rebuildImage(); });
    connect(state, &ViewState::viewChanged, this, [this] { if (paged_) rebuildImage(); });
}

void SpectrogramChart::setData(const std::vector<humtrace::ChannelView>* views, const humtrace::AnalysisReport* report,
                               const std::vector<humtrace::ui::FindingRow>* findings) {
    paged_ = nullptr;
    imageFirst_ = 0; imageStep_ = 1;
    views_ = views;
    report_ = report;
    findings_ = findings;
    selectedFinding_ = -1;
    rebuildImage();
}

void SpectrogramChart::setPagedData(PagedData* data, const std::vector<humtrace::ChannelView>* views,
    const humtrace::AnalysisReport* report, const std::vector<humtrace::ui::FindingRow>* findings) {
    paged_ = data; views_ = views; report_ = report; findings_ = findings;
    selectedFinding_ = -1;
    rebuildImage();
}
void SpectrogramChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (paged_) rebuildImage();
}
void SpectrogramChart::rebuildPagedImage() {
    image_ = QImage();
    const auto channel = state_->channel();
    if (!report_ || !views_ || channel < 0 || channel >= static_cast<int>(views_->size())) return;
    const auto c = static_cast<std::size_t>(channel);
    const auto count = paged_->storage().frame_count(c);
    const auto& edges = (*views_)[c].row_frequency_hz;
    if (count == 0 || edges.empty()) return;
    const double hop = static_cast<double>(report_->hop_size_samples) / report_->sample_rate_hz;
    const double offset = static_cast<double>(report_->frame_size_samples - report_->hop_size_samples) / (2.0 * report_->sample_rate_hz);
    const auto range = state_->view();
    const auto first = static_cast<std::uint64_t>(std::clamp(std::floor((range.start - offset) / hop), 0.0, static_cast<double>(count - 1)));
    const auto last = static_cast<std::uint64_t>(std::clamp(std::ceil((range.end - offset) / hop) + 1, static_cast<double>(first + 1), static_cast<double>(count)));
    const auto pixels = static_cast<std::uint64_t>(std::clamp(plotRect().width(), 1, 2047));
    unsigned level = 0;
    std::uint64_t scale = 1;
    while ((last - first + scale - 1) / scale > pixels) { scale *= 2; ++level; }
    const auto first_bucket = first / scale;
    const auto last_bucket = (last + scale - 1) / scale;
    const int columns = static_cast<int>(last_bucket - first_bucket);
    const int height = std::min(256, static_cast<int>(edges.size()));
    image_ = QImage(columns, height, QImage::Format_RGB32);
    if (image_.isNull()) throw std::runtime_error("could not allocate bounded spectrogram image");
    imageFirst_ = first_bucket * scale; imageStep_ = scale;
    const double nyquist = report_->sample_rate_hz / 2.0;
    for (int x = 0; x < columns; ++x) {
        const auto& row = paged_->row(c, first_bucket + static_cast<std::uint64_t>(x), level);
        for (int y = 0; y < height; ++y) {
            std::size_t low{}, high{};
            if (log_) {
                low = static_cast<std::size_t>((height - 1 - y) * static_cast<int>(row.size()) / height);
                high = static_cast<std::size_t>((height - y) * static_cast<int>(row.size()) / height);
            } else {
                const double lo = nyquist * (1.0 - static_cast<double>(y + 1) / height);
                const double hi = nyquist * (1.0 - static_cast<double>(y) / height);
                low = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, std::upper_bound(edges.begin(), edges.end(), lo) - edges.begin() - 1));
                high = static_cast<std::size_t>(std::upper_bound(edges.begin(), edges.end(), hi) - edges.begin());
            }
            low = std::min(low, row.size() - 1); high = std::clamp(high, low + 1, row.size());
            const auto peak = *std::max_element(row.begin() + static_cast<std::ptrdiff_t>(low), row.begin() + static_cast<std::ptrdiff_t>(high));
            const auto color = humtrace::ui::colormap(humtrace::ui::normalize_db(peak, floor_, ceiling_), palette_);
            image_.setPixel(x, y, qRgb(color.r, color.g, color.b));
        }
    }
}

void SpectrogramChart::setDisplay(bool logFrequency, humtrace::ui::Palette palette, double floorDb, double ceilingDb) {
    log_ = logFrequency;
    palette_ = palette;
    floor_ = floorDb;
    ceiling_ = std::max(ceilingDb, floorDb + 1.0);
    rebuildImage();
}

void SpectrogramChart::rebuildImage() {
    if (paged_) {
        try { rebuildPagedImage(); }
        catch (const std::exception& error) { image_ = QImage(); emit readFailed(QString::fromUtf8(error.what())); }
        update();
        return;
    }
    imageFirst_ = 0; imageStep_ = 1;
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
    // Column i is the window [i*hop, i*hop + window); draw it centered on the window center,
    // spanning one hop, so the column under the cursor is the frame the spectrum shows.
    const double offset = (windowSec - hopSec) / 2.0 + static_cast<double>(imageFirst_) * hopSec;
    const double imageHop = hopSec * static_cast<double>(imageStep_);
    const int columns = image_.width();
    const int first = std::clamp(static_cast<int>(std::floor((view.start - offset) / imageHop)) - 1, 0, columns - 1);
    const int last = std::clamp(static_cast<int>(std::ceil((view.end - offset) / imageHop)) + 1, first + 1, columns);
    // Crop to the visible columns: stretching the whole image at high zoom would need enormous rectangles.
    const QRectF target(xAt(offset + first * imageHop), plot.top(), xAt(offset + last * imageHop) - xAt(offset + first * imageHop), plot.height());
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(target, image_, QRectF(first, 0, last - first, image_.height()));

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
    if (paged_) p.drawText(plot.adjusted(4, 18, -4, 0), Qt::AlignLeft | Qt::AlignTop, tr("Display: maximum across %1 frame(s) per column; zoom for individual frames").arg(imageStep_));
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
