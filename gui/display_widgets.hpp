// Purpose: QPainter views of the precomputed display products (waveform envelope,
// log-frequency spectrogram with tone-track overlay). Display only: reads an
// AnalysisReport, never recomputes analysis; painting uses cached images.
#pragma once

#include "humtrace/report.hpp"

#include <QImage>
#include <QRgb>
#include <QWidget>

// Perceptual (viridis-like) colormap for t in [0,1], interpolated from fixed stops.
QRgb perceptualColor(double t);

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    void setReport(const humtrace::AnalysisReport* report, int channelIndex);
    void setSelectedFrame(int frameIndex);
    bool hasData() const { return envelope() != nullptr; }
    QSize sizeHint() const override { return {640, 220}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    const humtrace::WaveformEnvelope* envelope() const;
    const humtrace::AnalysisReport* report_{nullptr};
    int channel_{0};
    int selected_{-1};
};

class SpectrogramWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr double kFloorDbfs = -120.0;
    static constexpr double kCeilDbfs = 0.0;
    explicit SpectrogramWidget(QWidget* parent = nullptr);
    void setReport(const humtrace::AnalysisReport* report, int channelIndex);
    void setSelectedFrame(int frameIndex);
    bool hasData() const { return spectrogram() != nullptr && !image_.isNull(); }
    int columnCount() const;
    QSize sizeHint() const override { return {640, 260}; }
signals:
    void frameClicked(int frameIndex);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    const humtrace::Spectrogram* spectrogram() const;
    int frameCount() const;
    QRect plotRect() const;
    double rowPosition(double frequencyHz) const;   // 0..rows, fractional
    void rebuildImage();
    const humtrace::AnalysisReport* report_{nullptr};
    int channel_{0};
    int selected_{-1};
    QImage image_;
};
