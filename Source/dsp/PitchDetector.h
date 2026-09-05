#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace vocalpilot {
struct PitchEstimate { float hz = 0, confidence = 0; };

// Streaming, decimated YIN difference function. All storage is fixed-size.
class PitchDetector {
public:
    void prepare(double sampleRate) noexcept {
        decimation = std::max(1, static_cast<int>(sampleRate / 12000.0));
        rate = sampleRate / decimation;
        filterAlpha = 1.0f - static_cast<float>(std::exp(-2.0 * 3.141592653589793 * 1600.0 / sampleRate));
        minLag = std::max(2, static_cast<int>(rate / 1000.0));
        maxLag = std::min(450, static_cast<int>(rate / 65.0));
        hop = std::max(1, static_cast<int>(rate * 0.010));
        reset();
    }
    void reset() noexcept { ring.fill(0); write = filled = counter = hopCounter = 0; low1 = low2 = 0; estimate = {}; }
    bool push(float sample) noexcept {
        low1 += filterAlpha * (sample - low1);
        low2 += filterAlpha * (low1 - low2);
        if (++counter < decimation) return false;
        counter = 0; ring[write] = low2; write = (write + 1) % frame;
        filled = std::min(frame, filled + 1);
        if (++hopCounter < hop) return false;
        hopCounter = 0;
        if (filled < frame) return false;
        analyse(); return true;
    }
    PitchEstimate get() const noexcept { return estimate; }
private:
    static constexpr int frame = 1024, window = 512;
    std::array<float, frame> ring {}, data {};
    std::array<double, 452> difference {};
    int write = 0, filled = 0, counter = 0, hopCounter = 0, decimation = 4, hop = 120;
    int minLag = 12, maxLag = 184;
    double rate = 12000;
    float low1 = 0, low2 = 0, filterAlpha = 0.2f;
    PitchEstimate estimate;
    void analyse() noexcept {
        double energy = 0;
        for (int i = 0; i < frame; ++i) { data[i] = ring[(write + i) % frame]; energy += data[i] * data[i]; }
        estimate = {};
        if (energy / frame < 0.00001) return; // roughly -50 dBFS RMS after low-pass
        difference[0] = 1;
        double cumulative = 0;
        for (int lag = 1; lag <= maxLag + 1; ++lag) {
            double sum = 0;
            for (int i = 0; i < window; ++i) {
                const double delta = data[i] - data[i + lag]; sum += delta * delta;
            }
            cumulative += sum;
            difference[lag] = cumulative > 1e-20 ? sum * lag / cumulative : 1.0;
        }
        for (int lag = minLag; lag <= maxLag; ++lag) {
            if (difference[lag] >= 0.15) continue;
            while (lag < maxLag && difference[lag + 1] < difference[lag]) ++lag;
            const double a = difference[lag - 1], b = difference[lag], c = difference[lag + 1];
            const double denominator = a - 2 * b + c;
            const double offset = std::abs(denominator) > 1e-12 ? std::clamp(0.5 * (a - c) / denominator, -0.5, 0.5) : 0;
            const double hz = rate / (lag + offset);
            if (hz >= 65 && hz <= 1000) estimate = { static_cast<float>(hz), static_cast<float>(1 - b) };
            return;
        }
    }
};
}
