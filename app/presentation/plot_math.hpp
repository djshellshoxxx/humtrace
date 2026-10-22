#pragma once

// Toolkit-free plot math shared by the GUI widgets and unit tests.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace humtrace::ui {

struct TimeRange {
    double start{};
    double end{};
    [[nodiscard]] double length() const { return end - start; }
};

inline constexpr double kMinViewSeconds = 0.05;

// Clamp a range into [0, total], keeping its length when possible.
[[nodiscard]] inline TimeRange clamp_range(TimeRange r, double total) {
    const double min_len = std::min(kMinViewSeconds, std::max(total, 0.0));
    double length = std::clamp(r.length(), min_len, std::max(total, min_len));
    double start = std::clamp(r.start, 0.0, std::max(0.0, total - length));
    return {start, start + length};
}

// Zoom around `anchor` (seconds); factor < 1 zooms in.
[[nodiscard]] inline TimeRange zoom(TimeRange view, double anchor, double factor, double total) {
    const double a = std::clamp(anchor, view.start, view.end);
    const double fraction = view.length() > 0.0 ? (a - view.start) / view.length() : 0.5;
    const double length = view.length() * factor;
    return clamp_range({a - fraction * length, a - fraction * length + length}, total);
}

[[nodiscard]] inline TimeRange pan(TimeRange view, double delta, double total) {
    return clamp_range({view.start + delta, view.end + delta}, total);
}

[[nodiscard]] inline double map_linear(double v, double lo, double hi, double p0, double p1) {
    return hi == lo ? p0 : p0 + (v - lo) / (hi - lo) * (p1 - p0);
}

// Position in [0, 1] of `hz` on a log axis (clamped).
[[nodiscard]] inline double log_position(double hz, double min_hz, double max_hz) {
    if (!(hz > 0.0) || !(min_hz > 0.0) || !(max_hz > min_hz)) return 0.0;
    return std::clamp(std::log(hz / min_hz) / std::log(max_hz / min_hz), 0.0, 1.0);
}

[[nodiscard]] inline double hz_from_log_position(double position, double min_hz, double max_hz) {
    return min_hz * std::pow(max_hz / min_hz, std::clamp(position, 0.0, 1.0));
}

struct Rgb { std::uint8_t r{}, g{}, b{}; };
enum class Palette { Default, Grayscale };

// Map a normalized level in [0, 1] to a color. Default is a perceptually ordered
// dark-blue -> teal -> yellow ramp; Grayscale is the accessible alternative.
[[nodiscard]] inline Rgb colormap(double t, Palette palette) {
    t = std::clamp(t, 0.0, 1.0);
    if (palette == Palette::Grayscale) {
        const auto v = static_cast<std::uint8_t>(std::lround(255.0 * t));
        return {v, v, v};
    }
    static constexpr std::array<std::array<double, 3>, 5> stops{{
        {17, 24, 32}, {40, 72, 120}, {38, 140, 160}, {120, 210, 140}, {250, 235, 110}}};
    const double scaled = t * static_cast<double>(stops.size() - 1);
    const auto i = std::min<std::size_t>(static_cast<std::size_t>(scaled), stops.size() - 2);
    const double f = scaled - static_cast<double>(i);
    auto lerp = [&](std::size_t c) {
        return static_cast<std::uint8_t>(std::lround(stops[i][c] + f * (stops[i + 1][c] - stops[i][c])));
    };
    return {lerp(0), lerp(1), lerp(2)};
}

[[nodiscard]] inline double normalize_db(double db, double floor_db, double ceiling_db) {
    if (!std::isfinite(db) || !(ceiling_db > floor_db)) return 0.0;
    return std::clamp((db - floor_db) / (ceiling_db - floor_db), 0.0, 1.0);
}

// WCAG 2.x relative luminance contrast ratio between two sRGB colors.
[[nodiscard]] inline double contrast_ratio(Rgb a, Rgb b) {
    auto lum = [](Rgb c) {
        auto lin = [](std::uint8_t v) {
            const double s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
    };
    const double la = lum(a), lb = lum(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

} // namespace humtrace::ui
