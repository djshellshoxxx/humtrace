// Purpose: QPainter plots for HumTrace results (finding timeline, frame spectrum).
// Display only: reads an AnalysisReport, never recomputes analysis.
#pragma once

#include "humtrace/report.hpp"
#include "humtrace/spectrum.hpp"

#include <QWidget>
#include <vector>

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    void setReport(const humtrace::AnalysisReport* report, int channelIndex);
    void setSelectedFrame(int frameIndex);
    QSize sizeHint() const override { return {640, 180}; }
signals:
    void frameClicked(int frameIndex);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    int frameCount() const;
    const humtrace::AnalysisReport* report_{nullptr};
    int channel_{0};
    int selected_{-1};
};

class SpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget* parent = nullptr);
    void setSpectrum(std::vector<humtrace::SpectrumBin> bins);
    QSize sizeHint() const override { return {640, 180}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    std::vector<humtrace::SpectrumBin> bins_;
};
