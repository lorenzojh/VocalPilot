#pragma once
#include <array>
#include <cmath>
#include <limits>

namespace vocalpilot {
struct NoteTarget {
    static double midi(double hz) noexcept { return 69.0 + 12.0 * std::log2(hz / 440.0); }
    static double frequency(double note) noexcept { return 440.0 * std::exp2((note - 69.0) / 12.0); }
    static int pitchClass(int note) noexcept { return (note % 12 + 12) % 12; }
    static bool permitted(int note, int key, bool minor) noexcept {
        constexpr std::array<int, 7> major { 0, 2, 4, 5, 7, 9, 11 };
        constexpr std::array<int, 7> naturalMinor { 0, 2, 3, 5, 7, 8, 10 };
        for (auto interval : minor ? naturalMinor : major)
            if (pitchClass(note - key) == interval) return true;
        return false;
    }
    // Search across octave boundaries. An exact tie selects the lower note.
    static int nearest(double note, int key, bool minor) noexcept {
        int best = static_cast<int>(std::floor(note)) - 12;
        double distance = std::numeric_limits<double>::max();
        for (int n = best; n <= static_cast<int>(std::ceil(note)) + 12; ++n)
            if (permitted(n, key, minor) && std::abs(n - note) < distance) {
                distance = std::abs(n - note); best = n;
            }
        return best;
    }
};
}
