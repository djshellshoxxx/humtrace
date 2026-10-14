#pragma once

#include "humtrace/view.hpp"

#include <QWidget>

class ViewState;

// Spectrum of the frame under the cursor (decimated per log-frequency row; exact peak and
// harmonic values are in the table and inspector).
class SpectrumChart : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumChart(QWidget* parent = nullptr);
    void setFrame(const std::vector<float>* levels, const std::vector<double>* rowHz, double nyquistHz,
                  double floorDb, double ceilingDb);
    [[nodiscard]] QString readout() const { return readout_; }

signals:
    void readoutChanged(const QString& text);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void setHoverRow(int row);
    const std::vector<float>* levels_{nullptr};
    const std::vector<double>* rowHz_{nullptr};
    double nyquist_{0};
    double floor_{-100}, ceiling_{0};
    int hoverRow_{-1};
    QString readout_;
};
