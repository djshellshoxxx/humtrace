#pragma once

#include "../presentation/plot_math.hpp"

#include <QObject>

// Shared, display-only state: view range, cursor, selection, channel. Changing it never
// touches analysis results.
class ViewState : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] double total() const { return total_; }
    [[nodiscard]] humtrace::ui::TimeRange view() const { return view_; }
    [[nodiscard]] double cursor() const { return cursor_; }
    [[nodiscard]] bool hasSelection() const { return selEnd_ > selStart_; }
    [[nodiscard]] double selectionStart() const { return selStart_; }
    [[nodiscard]] double selectionEnd() const { return selEnd_; }
    [[nodiscard]] int channel() const { return channel_; } // 0-based
    [[nodiscard]] double step() const { return step_; }

    void reset(double totalSeconds, double stepSeconds);
    void setView(humtrace::ui::TimeRange range);
    void zoomAround(double anchor, double factor);
    void panBy(double delta);
    void setCursor(double seconds);
    void setSelection(double a, double b);
    void clearSelection();
    void setChannel(int channel);

signals:
    void viewChanged();
    void cursorChanged();
    void selectionChanged();
    void channelChanged();

private:
    double total_{0.0};
    humtrace::ui::TimeRange view_{};
    double cursor_{0.0};
    double selStart_{0.0};
    double selEnd_{0.0};
    double step_{1.0};
    int channel_{0};
};
