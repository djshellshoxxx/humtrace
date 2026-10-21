#include "humtrace/music.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace humtrace {

NoteInfo describe_note(double frequency_hz, double a4_hz) {
    if (!std::isfinite(frequency_hz) || frequency_hz <= 0.0 || !std::isfinite(a4_hz) || a4_hz <= 0.0)
        throw std::invalid_argument("frequency and A4 reference must be finite and positive");
    static constexpr const char* names[] = {"C", "C#", "D", "D#", "E", "F",
                                            "F#", "G", "G#", "A", "A#", "B"};
    const double exact = 69.0 + 12.0 * std::log2(frequency_hz / a4_hz);
    const int midi = static_cast<int>(std::lround(exact));
    NoteInfo info;
    info.midi_note = midi;
    info.cents = (exact - static_cast<double>(midi)) * 100.0;
    info.nearest_hz = a4_hz * std::pow(2.0, static_cast<double>(midi - 69) / 12.0);
    const int pitch_class = ((midi % 12) + 12) % 12;
    const int octave = (midi - pitch_class) / 12 - 1;
    info.name = std::string(names[pitch_class]) + std::to_string(octave);
    return info;
}

std::string format_note(const NoteInfo& note) {
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%s %+.0fc", note.name.c_str(), note.cents);
    return buffer;
}

double interval_cents(double from_hz, double to_hz) {
    if (!std::isfinite(from_hz) || !std::isfinite(to_hz) || from_hz <= 0.0 || to_hz <= 0.0)
        throw std::invalid_argument("frequencies must be finite and positive");
    return 1200.0 * std::log2(to_hz / from_hz);
}

} // namespace humtrace
