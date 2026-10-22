#include "humtrace/music.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace humtrace {

NoteInfo nearest_note(double frequency_hz, double a4_hz) {
    if (!(frequency_hz > 0.0) || !std::isfinite(frequency_hz) || !(a4_hz > 0.0) || !std::isfinite(a4_hz))
        throw std::invalid_argument("frequency and A4 reference must be positive and finite");
    static constexpr std::array<const char*, 12> names{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const double semitones = 69.0 + 12.0 * std::log2(frequency_hz / a4_hz);
    NoteInfo note;
    note.midi_note = static_cast<int>(std::lround(semitones));
    const int pitch_class = ((note.midi_note % 12) + 12) % 12;
    const int octave = (note.midi_note - pitch_class) / 12 - 1;
    note.name = std::string(names[static_cast<std::size_t>(pitch_class)]) + std::to_string(octave);
    note.nearest_hz = a4_hz * std::pow(2.0, (note.midi_note - 69) / 12.0);
    note.cents = 1200.0 * std::log2(frequency_hz / note.nearest_hz);
    return note;
}

std::string format_note(const NoteInfo& note) {
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%s %+.0fc", note.name.c_str(), note.cents);
    return buffer;
}

} // namespace humtrace
