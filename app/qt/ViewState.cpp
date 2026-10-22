#include "ViewState.hpp"

#include <algorithm>

void ViewState::reset(double totalSeconds, double stepSeconds) {
    total_ = std::max(0.0, totalSeconds);
    step_ = std::max(1e-6, stepSeconds);
    view_ = {0.0, total_};
    cursor_ = 0.0;
    selStart_ = selEnd_ = 0.0;
    channel_ = 0;
    emit viewChanged();
    emit cursorChanged();
    emit selectionChanged();
    emit channelChanged();
}

void ViewState::setView(humtrace::ui::TimeRange range) {
    view_ = humtrace::ui::clamp_range(range, total_);
    emit viewChanged();
}
void ViewState::zoomAround(double anchor, double factor) { setView(humtrace::ui::zoom(view_, anchor, factor, total_)); }
void ViewState::panBy(double delta) { setView(humtrace::ui::pan(view_, delta, total_)); }

void ViewState::setCursor(double seconds) {
    cursor_ = std::clamp(seconds, 0.0, total_);
    emit cursorChanged();
}
void ViewState::setSelection(double a, double b) {
    selStart_ = std::clamp(std::min(a, b), 0.0, total_);
    selEnd_ = std::clamp(std::max(a, b), 0.0, total_);
    emit selectionChanged();
}
void ViewState::clearSelection() {
    selStart_ = selEnd_ = 0.0;
    emit selectionChanged();
}
void ViewState::setChannel(int channel) {
    if (channel == channel_) return;
    channel_ = channel;
    emit channelChanged();
}
