#pragma once

#include <string>

namespace humtrace {

struct NoteInfo {
    int midi_note{};         // MIDI note number (A4 = 69)
    std::string name;        // Scientific pitch name, e.g. "A#1"
    double nearest_hz{};     // Equal-tempered frequency of that note
    double cents{};          // Signed deviation of the input from the note
};

// Map a positive frequency to the nearest equal-tempered note for the given
// A4 reference (default 440 Hz). Throws std::invalid_argument on bad input.
[[nodiscard]] NoteInfo nearest_note(double frequency_hz, double a4_hz = 440.0);

// "A#1 +12c" style label for display.
[[nodiscard]] std::string format_note(const NoteInfo& note);

} // namespace humtrace
