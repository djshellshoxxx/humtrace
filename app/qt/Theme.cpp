#include "Theme.hpp"

#include <QApplication>
#include <QPalette>

namespace theme {

Tokens tokens(bool hc) {
    if (hc)
        return {QColor("#000000"), QColor("#000000"), QColor("#1A1A1A"), QColor("#FFFFFF"),
                QColor("#FFFFFF"), QColor("#00FFFF"), QColor("#FFD000"), QColor("#FF80FF"),
                QColor("#00FF80"), QColor("#FF6060"), QColor("#FFFF00")};
    return {QColor("#111820"), QColor("#19232D"), QColor("#222F3B"), QColor("#F0F3F6"),
            QColor("#A6B3C2"), QColor("#59D8E8"), QColor("#FFBF69"), QColor("#C4A7FF"),
            QColor("#7ED6A5"), QColor("#FF7773"), QColor("#59D8E8")};
}

namespace { bool g_highContrast = false; }
Tokens current() { return tokens(g_highContrast); }

void apply(QApplication& app, bool hc) {
    g_highContrast = hc;
    const auto t = tokens(hc);
    QPalette p;
    p.setColor(QPalette::Window, t.canvas);
    p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.surface);
    p.setColor(QPalette::AlternateBase, t.raised);
    p.setColor(QPalette::Text, t.text);
    p.setColor(QPalette::Button, t.raised);
    p.setColor(QPalette::ButtonText, t.text);
    p.setColor(QPalette::Highlight, t.signal);
    p.setColor(QPalette::HighlightedText, QColor("#000000"));
    p.setColor(QPalette::ToolTipBase, t.raised);
    p.setColor(QPalette::ToolTipText, t.text);
    p.setColor(QPalette::PlaceholderText, t.textSecondary);
    p.setColor(QPalette::Disabled, QPalette::Text, t.textSecondary);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.textSecondary);
    app.setStyle("Fusion");
    app.setPalette(p);
    app.setStyleSheet(QString(
        "QWidget:focus { outline: none; }"
        "QPushButton:focus, QTableView:focus, QTextBrowser:focus, QTabBar::tab:focus, QToolButton:focus {"
        "  border: 2px solid %1; }"
        "QToolBar { background: %2; border: 0; spacing: 4px; }"
        "QToolButton { padding: 6px 10px; color: %3; }"
        "QToolButton:disabled { color: %4; }"
        "QHeaderView::section { background: %5; color: %3; padding: 4px; border: 0; }"
        "QStatusBar { background: %2; color: %3; }"
        "QDockWidget { color: %3; }"
        "QTabBar::tab { padding: 6px 12px; background: %2; color: %3; }"
        "QTabBar::tab:selected { background: %5; }")
        .arg(t.focus.name(), t.surface.name(), t.text.name(), t.textSecondary.name(), t.raised.name()));
}

} // namespace theme
