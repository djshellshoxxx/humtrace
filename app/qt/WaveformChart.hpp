#pragma once

#include "TimeChart.hpp"
#include "humtrace/view.hpp"

#include <vector>

class WaveformChart : public TimeChart {
    Q_OBJECT
public:
    explicit WaveformChart(ViewState* state, QWidget* parent = nullptr);
    void setViews(const std::vector<humtrace::ChannelView>* views, double durationSeconds, double sampleRateHz = 1.0);

protected:
    void paintData(QPainter& p, const QRect& plot) override;

private:
    const std::vector<humtrace::ChannelView>* views_{nullptr};
    double duration_{0.0};
    double sampleRate_{1.0};
};
