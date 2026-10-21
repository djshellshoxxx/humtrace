#pragma once

#include <string>

namespace humtrace {

struct NoteInfo {
    int midi_note{};          // nearest equal-tempered note, A4 = 69
    std::string name;         // scientific pitch notation with sharps, e.g. "B1"
    double cents{};           // deviation from that note, in [-50, 50]
    double nearest_hz{};      // frequency of the nearest note
};

// Map a frequency to the nearest 12-tone equal-tempered note. Throws
// std::invalid_argument for non-finite or non-positive frequencies or A4.
[[nodiscard]] NoteInfo describe_note(double frequency_hz, double a4_hz = 440.0);

// "B1 -23c" style label for console output.
[[nodiscard]] std::string format_note(const NoteInfo& note);

// Signed interval from `from_hz` to `to_hz` in cents (1200 per octave).
[[nodiscard]] double interval_cents(double from_hz, double to_hz);

} // namespace humtrace
