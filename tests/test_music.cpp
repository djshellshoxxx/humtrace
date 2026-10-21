#include "humtrace/music.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
} // namespace

int main() {
    using namespace humtrace;
    auto a4 = describe_note(440.0);
    require(a4.name == "A4" && a4.midi_note == 69 && std::abs(a4.cents) < 1e-9, "A4 is exact");
    require(describe_note(261.6256).name == "C4", "middle C");
    require(describe_note(27.5).name == "A0", "lowest piano key");
    auto mains60 = describe_note(60.0);
    require(mains60.name == "B1" && mains60.cents < -45.0 && mains60.cents > -50.0, "60 Hz sits almost midway between A#1 and B1");
    require(describe_note(50.0).name == "G1", "50 Hz is near G1");
    require(describe_note(445.0).cents > 0.0 && describe_note(445.0).name == "A4", "sharp A4 keeps its name");
    require(describe_note(432.0, 432.0).name == "A4", "alternate A4 reference");
    require(describe_note(466.1638).name == "A#4", "sharps naming");
    require(std::abs(interval_cents(440.0, 880.0) - 1200.0) < 1e-9, "octave is 1200 cents");
    require(format_note(a4) == "A4 +0c", "label format");
    bool threw = false;
    try { (void)describe_note(0.0); } catch (const std::invalid_argument&) { threw = true; }
    require(threw, "zero frequency rejected");
    threw = false;
    try { (void)describe_note(440.0, -1.0); } catch (const std::invalid_argument&) { threw = true; }
    require(threw, "negative A4 rejected");
    std::cout << "All music tests passed.\n";
}
