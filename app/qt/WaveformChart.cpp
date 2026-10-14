#include "WaveformChart.hpp"

#include "Theme.hpp"

#include <QPainter>
#include <algorithm>

WaveformChart::WaveformChart(ViewState* state, QWidget* parent)
    : TimeChart(state, tr("Waveform overview"), parent) {}

void WaveformChart::setViews(const std::vector<humtrace::ChannelView>* views, double durationSeconds) {
    views_ = views;
    duration_ = durationSeconds;
    update();
}

void WaveformChart::paintData(QPainter& p, const QRect& plot) {
    if (!views_ || views_->empty() || duration_ <= 0.0) return;
    const auto t = theme::tokens(false);
    const int lanes = static_cast<int>(views_->size());
    const int laneHeight = plot.height() / lanes;
    const auto view = state_->view();
    for (int lane = 0; lane < lanes; ++lane) {
        const QRect laneRect(plot.left(), plot.top() + lane * laneHeight, plot.width(), laneHeight);
        const bool selected = lane == state_->channel();
        p.setPen(QPen(t.raised, 1));
        p.drawLine(laneRect.left(), laneRect.center().y(), laneRect.right(), laneRect.center().y());
        // Selected channel solid, others dashed: color is never the only encoding.
        p.setPen(QPen(selected ? t.signal : t.textSecondary, 1, selected ? Qt::SolidLine : Qt::DashLine));
        p.drawText(laneRect.adjusted(4, 2, 0, 0), Qt::AlignLeft | Qt::AlignTop, tr("Ch %1").arg(lane + 1));
        const auto& v = (*views_)[static_cast<std::size_t>(lane)];
        const auto buckets = v.waveform_min.size();
        if (buckets == 0) continue;
        const double secondsPerBucket = duration_ / static_cast<double>(buckets);
        for (int x = laneRect.left(); x <= laneRect.right(); ++x) {
            // Aggregate all buckets covered by this pixel column (min of mins, max of maxes).
            const double t0 = humtrace::ui::map_linear(x, laneRect.left(), laneRect.right(), view.start, view.end);
            const double t1 = humtrace::ui::map_linear(x + 1, laneRect.left(), laneRect.right(), view.start, view.end);
            auto b0 = static_cast<long long>(t0 / secondsPerBucket);
            auto b1 = std::max<long long>(b0, static_cast<long long>(t1 / secondsPerBucket));
            if (b1 < 0 || b0 >= static_cast<long long>(buckets)) continue;
            b0 = std::max<long long>(b0, 0);
            b1 = std::min<long long>(b1, static_cast<long long>(buckets) - 1);
            float lo = v.waveform_min[static_cast<std::size_t>(b0)], hi = v.waveform_max[static_cast<std::size_t>(b0)];
            for (auto b = b0 + 1; b <= b1; ++b) {
                lo = std::min(lo, v.waveform_min[static_cast<std::size_t>(b)]);
                hi = std::max(hi, v.waveform_max[static_cast<std::size_t>(b)]);
            }
            const double mid = laneRect.center().y(), half = laneHeight / 2.0 - 2.0;
            p.drawLine(x, static_cast<int>(mid - std::clamp<double>(hi, -1, 1) * half),
                       x, static_cast<int>(mid - std::clamp<double>(lo, -1, 1) * half));
        }
    }
}
