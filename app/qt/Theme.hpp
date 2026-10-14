#pragma once

#include <QColor>
#include <QString>

class QApplication;

namespace theme {
struct Tokens {
    QColor canvas, surface, raised, text, textSecondary, signal, event, compare, ok, error, focus;
};
[[nodiscard]] Tokens tokens(bool highContrast);
// Tokens of the theme most recently applied (charts paint with these).
[[nodiscard]] Tokens current();
// Apply the dark instrument palette and a stylesheet with visible focus rings.
void apply(QApplication& app, bool highContrast);
} // namespace theme
