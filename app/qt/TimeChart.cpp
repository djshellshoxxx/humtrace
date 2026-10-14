#include "TimeChart.hpp"

#include "Theme.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

TimeChart::TimeChart(ViewState* state, const QString& accessibleName, QWidget* parent)
    : QWidget(parent), state_(state) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(accessibleName);
    connect(state, &ViewState::viewChanged, this, qOverload<>(&QWidget::update));
    connect(state, &ViewState::cursorChanged, this, qOverload<>(&QWidget::update));
    connect(state, &ViewState::selectionChanged, this, qOverload<>(&QWidget::update));
    connect(state, &ViewState::channelChanged, this, qOverload<>(&QWidget::update));
}

void TimeChart::setAccessibleSummary(const QString& summary) {
    setAccessibleDescription(summary);
    setToolTip(summary);
}

QRect TimeChart::plotRect() const { return QRect(kLeft, 4, std::max(10, width() - kLeft - 8), std::max(10, height() - kBottom - 6)); }

double TimeChart::timeAt(int x) const {
    const auto r = plotRect();
    const auto v = state_->view();
    return humtrace::ui::map_linear(x, r.left(), r.right(), v.start, v.end);
}
double TimeChart::xAt(double seconds) const {
    const auto r = plotRect();
    const auto v = state_->view();
    return humtrace::ui::map_linear(seconds, v.start, v.end, r.left(), r.right());
}

void TimeChart::drawTimeRuler(QPainter& p) const {
    const auto r = plotRect();
    const auto t = theme::tokens(false);
    const auto v = state_->view();
    p.setPen(t.textSecondary);
    // Pick a "nice" tick step giving roughly one tick per 90 px.
    const double target = v.length() * 90.0 / std::max(1, r.width());
    const double mag = std::pow(10.0, std::floor(std::log10(std::max(target, 1e-9))));
    double step = mag;
    for (const double m : {1.0, 2.0, 5.0, 10.0}) if (m * mag >= target) { step = m * mag; break; }
    for (double s = std::ceil(v.start / step) * step; s <= v.end + 1e-9; s += step) {
        const int x = static_cast<int>(xAt(s));
        p.drawLine(x, r.bottom(), x, r.bottom() + 4);
        p.drawText(QRect(x - 40, r.bottom() + 4, 80, kBottom - 4), Qt::AlignHCenter | Qt::AlignTop,
                   QString::number(s, 'f', step < 1.0 ? 2 : 0) + " s");
    }
}

void TimeChart::drawCursorAndSelection(QPainter& p) const {
    const auto r = plotRect();
    const auto t = theme::tokens(false);
    if (state_->hasSelection()) {
        const int a = static_cast<int>(xAt(state_->selectionStart()));
        const int b = static_cast<int>(xAt(state_->selectionEnd()));
        QColor fill = t.event;
        fill.setAlpha(16);
        p.fillRect(QRect(QPoint(a, r.top()), QPoint(b, r.bottom())), fill);
        p.setPen(QPen(t.event, 1, Qt::DashLine));
        p.drawLine(a, r.top(), a, r.bottom());
        p.drawLine(b, r.top(), b, r.bottom());
    }
    const double c = state_->cursor();
    if (c >= state_->view().start && c <= state_->view().end) {
        p.setPen(QPen(t.signal, 2, Qt::SolidLine));
        const int x = static_cast<int>(xAt(c));
        p.drawLine(x, r.top(), x, r.bottom());
    }
}

void TimeChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const auto t = theme::tokens(false);
    p.fillRect(rect(), t.surface);
    const auto r = plotRect();
    if (state_->total() <= 0.0) {
        p.setPen(t.textSecondary);
        p.drawText(r, Qt::AlignCenter, tr("Open a recording and run Analyze"));
    } else {
        p.setClipRect(r);
        paintData(p, r);
        drawCursorAndSelection(p);
        p.setClipping(false);
        paintAxes(p, r);
        drawTimeRuler(p);
    }
    p.setPen(QPen(hasFocus() ? t.focus : t.raised, hasFocus() ? 3 : 1));
    p.drawRect(r.adjusted(-1, -1, 0, 0));
}

void TimeChart::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (e->button() != Qt::LeftButton || state_->total() <= 0.0) return;
    dragAnchor_ = timeAt(e->position().toPoint().x());
    dragging_ = true;
    state_->setCursor(dragAnchor_);
    if (!(e->modifiers() & Qt::ShiftModifier)) state_->clearSelection();
}
void TimeChart::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging_) return;
    const double t = timeAt(e->position().toPoint().x());
    if (std::abs(t - dragAnchor_) > state_->view().length() * 0.003) state_->setSelection(dragAnchor_, t);
}
void TimeChart::mouseReleaseEvent(QMouseEvent*) { dragging_ = false; }

void TimeChart::wheelEvent(QWheelEvent* e) {
    if (state_->total() <= 0.0) return;
    if (e->angleDelta().x() != 0 && std::abs(e->angleDelta().x()) > std::abs(e->angleDelta().y()))
        state_->panBy(-e->angleDelta().x() / 120.0 * state_->view().length() * 0.1);
    else
        state_->zoomAround(timeAt(static_cast<int>(e->position().x())), e->angleDelta().y() > 0 ? 0.8 : 1.25);
    e->accept();
}

void TimeChart::keyPressEvent(QKeyEvent* e) {
    if (state_->total() <= 0.0) { QWidget::keyPressEvent(e); return; }
    const double step = state_->step() * (e->modifiers() & Qt::ControlModifier ? 10.0 : 1.0);
    const double before = state_->cursor();
    switch (e->key()) {
    case Qt::Key_Left: case Qt::Key_Right: {
        const double next = before + (e->key() == Qt::Key_Left ? -step : step);
        if (e->modifiers() & Qt::ShiftModifier) {
            const double anchor = state_->hasSelection() ? (std::abs(state_->selectionStart() - before) < 1e-9 ? state_->selectionEnd() : state_->selectionStart()) : before;
            state_->setCursor(next);
            state_->setSelection(anchor, state_->cursor());
        } else {
            state_->setCursor(next);
        }
        if (state_->cursor() < state_->view().start || state_->cursor() > state_->view().end)
            state_->panBy(state_->cursor() - (state_->cursor() < state_->view().start ? state_->view().start : state_->view().end));
        break;
    }
    case Qt::Key_Plus: case Qt::Key_Equal: state_->zoomAround(state_->cursor(), 0.8); break;
    case Qt::Key_Minus: state_->zoomAround(state_->cursor(), 1.25); break;
    case Qt::Key_Home: state_->setCursor(0.0); state_->setView({0.0, state_->view().length()}); break;
    case Qt::Key_End: state_->setCursor(state_->total()); state_->setView({state_->total() - state_->view().length(), state_->total()}); break;
    case Qt::Key_0: state_->setView({0.0, state_->total()}); break;
    case Qt::Key_Escape: state_->clearSelection(); break;
    default: QWidget::keyPressEvent(e); return;
    }
    e->accept();
}
