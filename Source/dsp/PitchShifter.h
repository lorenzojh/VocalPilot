#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vocalpilot {
// Two fractional delay taps with complementary raised-cosine windows.
// Stereo uses the same phase; no allocation occurs in process().
class PitchShifter {
public:
    void prepare(double rate) {
        span = std::max(64, static_cast<int>(rate * 0.040));
        latency = 32 + span / 2;
        size = span + 68;
        for (auto& channel : buffer) channel.assign(static_cast<size_t>(size), 0);
        write = 0; phase = 0;
    }
    int latencySamples() const noexcept { return latency; }
    void process(const float* input, float* shifted, float* dry, int channels, double ratio) noexcept {
        for (int ch = 0; ch < channels; ++ch) buffer[ch][write] = input[ch];
        const double other = phase < 0.5 ? phase + 0.5 : phase - 0.5;
        const float weight = static_cast<float>(0.5 - 0.5 * std::cos(6.283185307179586 * phase));
        for (int ch = 0; ch < channels; ++ch) {
            shifted[ch] = weight * read(ch, 32 + phase * span) + (1 - weight) * read(ch, 32 + other * span);
            dry[ch] = read(ch, latency);
        }
        phase += (1.0 - std::clamp(ratio, 0.5, 2.0)) / span;
        phase -= std::floor(phase);
        write = (write + 1) % size;
    }
private:
    std::array<std::vector<float>, 2> buffer;
    int size = 0, span = 0, write = 0, latency = 0;
    double phase = 0;
    float read(int channel, double delay) const noexcept {
        double position = write - delay;
        if (position < 0) position += size;
        const int index = static_cast<int>(position);
        const float fraction = static_cast<float>(position - index);
        return buffer[channel][index] + fraction * (buffer[channel][(index + 1) % size] - buffer[channel][index]);
    }
};
}
