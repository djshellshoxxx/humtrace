#pragma once

#include "ViewState.hpp"

#include <QWidget>

// Base class for charts sharing a time axis: mouse/keyboard navigation, cursor and
// selection drawing, and accessible naming. Subclasses paint their own data.
class TimeChart : public QWidget {
    Q_OBJECT
public:
    TimeChart(ViewState* state, const QString& accessibleName, QWidget* parent = nullptr);
    void setAccessibleSummary(const QString& summary);

protected:
    static constexpr int kLeft = 56;   // room for y labels
    static constexpr int kBottom = 22; // room for the time ruler
    [[nodiscard]] QRect plotRect() const;
    [[nodiscard]] double timeAt(int x) const;
    [[nodiscard]] double xAt(double seconds) const;
    void drawTimeRuler(QPainter& p) const;
    void drawCursorAndSelection(QPainter& p) const;

    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void paintEvent(QPaintEvent*) override;
    virtual void paintData(QPainter& p, const QRect& plot) = 0;
    // Axis labels live outside the plot rectangle, so they are drawn unclipped.
    virtual void paintAxes(QPainter&, const QRect&) {}

    ViewState* state_;

private:
    double dragAnchor_{0.0};
    bool dragging_{false};
};
