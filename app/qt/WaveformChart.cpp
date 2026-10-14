#include "WaveformChart.hpp"

#include "Theme.hpp"

#include <QPainter>
#include <algorithm>

WaveformChart::WaveformChart(ViewState* state, QWidget* parent)
    : TimeChart(state, tr("Waveform overview"), parent) {}

void WaveformChart::setViews(const std::vector<humtrace::ChannelView>* views, double durationSeconds, double sampleRateHz) {
    views_ = views;
    sampleRate_ = sampleRateHz > 0.0 ? sampleRateHz : 1.0;
    duration_ = durationSeconds;
    update();
}

void WaveformChart::paintData(QPainter& p, const QRect& plot) {
    if (!views_ || views_->empty() || duration_ <= 0.0) return;
    const auto t = theme::current();
    // Too many channels for readable lanes: show only the selected channel (label says which).
    constexpr int kMinLaneHeight = 24;
    const int channelCount = static_cast<int>(views_->size());
    const bool single = plot.height() / channelCount < kMinLaneHeight;
    const int lanes = single ? 1 : channelCount;
    const int laneHeight = plot.height() / lanes;
    const auto view = state_->view();
    for (int lane = 0; lane < lanes; ++lane) {
        const int channel = single ? std::clamp(state_->channel(), 0, channelCount - 1) : lane;
        const QRect laneRect(plot.left(), plot.top() + lane * laneHeight, plot.width(), laneHeight);
        const bool selected = channel == state_->channel();
        p.setPen(QPen(t.raised, 1));
        p.drawLine(laneRect.left(), laneRect.center().y(), laneRect.right(), laneRect.center().y());
        // Selected channel solid, others dashed: color is never the only encoding.
        p.setPen(QPen(selected ? t.signal : t.textSecondary, 1, selected ? Qt::SolidLine : Qt::DashLine));
        p.drawText(laneRect.adjusted(4, 2, 0, 0), Qt::AlignLeft | Qt::AlignTop, single ? tr("Ch %1 of %2 (selected; resize or pick another channel)").arg(channel + 1).arg(channelCount) : tr("Ch %1").arg(channel + 1));
        const auto& v = (*views_)[static_cast<std::size_t>(channel)];
        const auto buckets = v.waveform_min.size();
        if (buckets == 0) continue;
        const double secondsPerBucket = static_cast<double>(v.waveform_samples_per_bucket) / sampleRate_;
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
            const double mid = laneRect.center().y(), half = std::max(1.0, laneHeight / 2.0 - 2.0);
            p.drawLine(x, static_cast<int>(mid - std::clamp<double>(hi, -1, 1) * half),
                       x, static_cast<int>(mid - std::clamp<double>(lo, -1, 1) * half));
        }
    }
}
