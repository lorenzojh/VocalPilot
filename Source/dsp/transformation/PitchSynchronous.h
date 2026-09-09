#pragma once
#include "Transformation.h"

namespace vocalpilot {
// TD pitch-synchronous OLA: unresampled, locally refined waveform epochs are
// placed on a separate synthesis clock. Spectral resonances within each grain
// remain at their original frequencies. This is not the legacy moving delay.
class PitchSynchronous final : public IPitchTransformationEngine {
public:
    void prepare(double sampleRate, int latency) override {
        rate = sampleRate; delay = latency; maxPeriod = rate / 64.0;
        capacity = 4 * delay + 32; history.prepare(capacity);
        for (auto& x : sum) x.assign(capacity, 0);
        weight.assign(capacity, 0); marks.fill(-1);
        clock = 0; markCount = 0; predicted = -1; synthesis = 0; mix = 0;
        slew = 1 - std::exp(-1 / (.003 * rate));
    }
    void setEpochRefinement(bool enabled) noexcept { refine = enabled; }
    void process(const float* input, float* output, int channels, const TransformationControl& request) noexcept override {
        const auto c = cleanControl(request);
        history.put(clock, input, channels, c);
        if (c.voiced) {
            const double period = rate / c.sourceHz;
            if (predicted < 0) predicted = static_cast<double>(clock);
            if (clock >= predicted + .25 * period + 2) {
                double epoch = predicted;
                if (refine) {
                    const int64_t a = static_cast<int64_t>(std::ceil(predicted - .22 * period));
                    const int64_t b = static_cast<int64_t>(std::floor(predicted + .22 * period));
                    int64_t peak = std::max(int64_t{1}, a);
                    for (auto i = peak + 1; i <= b; ++i)
                        if (history.at(0, i) > history.at(0, peak)) peak = i;
                    const double l = history.at(0, peak - 1), m = history.at(0, peak), r = history.at(0, peak + 1);
                    const double denominator = l - 2 * m + r;
                    epoch = static_cast<double>(peak) + (denominator < -1e-12 ? std::clamp(.5 * (l - r) / denominator, -.5, .5) : 0);
                }
                marks[markCount++ % marks.size()] = epoch;
                predicted = epoch + period;
            }
        } else predicted = -1;

        // Wait for analysis support and the nearest future epoch. Maximum work
        // is one grain per sample: minimum synthesis period is rate/2010 > 3.
        if (clock >= synthesis + 3 * maxPeriod + 4) {
            const auto s = history.control(static_cast<int64_t>(synthesis));
            if (s.voiced) {
                const double period = rate / s.sourceHz, outputPeriod = period / s.ratio;
                double epoch = -1, distance = 1e30;
                const auto count = std::min(markCount, marks.size());
                for (size_t i = 0; i < count; ++i) {
                    const double d = std::abs(marks[i] - synthesis);
                    if (d < distance) { distance = d; epoch = marks[i]; }
                }
                if (epoch >= 0 && distance < .8 * period) {
                    // Keep a two-source-period window. Widening it to the
                    // synthesis period suppresses the new fundamental at an
                    // octave down. Per-sample window normalization similarly
                    // erases the amplitude structure that creates new epochs.
                    const double radius = period;
                    const double center = synthesis + delay;
                    const auto begin = static_cast<int64_t>(std::ceil(center - radius));
                    const auto end = static_cast<int64_t>(std::floor(center + radius));
                    for (auto t = begin; t <= end; ++t) {
                        if (t < clock) continue;
                        const double offset = static_cast<double>(t) - center;
                        const float w = static_cast<float>(.5 + .5 * std::cos(transformPi * offset / radius));
                        const int j = index(t);
                        weight[j] = 1; // coverage, not an amplitude divisor
                        for (int ch = 0; ch < channels; ++ch)
                            sum[ch][j] += w * static_cast<float>(1 / std::sqrt(s.ratio)) * history.at(ch, epoch + offset);
                    }
                }
                synthesis += outputPeriod;
            } else synthesis += std::max(1.0, rate * .001);
        }
        const int j = index(clock);
        const auto delayed = history.control(clock - delay);
        const bool active = delayed.voiced && std::abs(delayed.ratio - 1) > 1e-10 && weight[j] > .05f;
        mix += slew * ((active ? delayed.amount : 0) - mix);
        if (mix < 1e-7) mix = 0;
        for (int ch = 0; ch < channels; ++ch) {
            const float dry = history.at(ch, clock - delay);
            const float wet = weight[j] > .05f ? sum[ch][j] : dry;
            output[ch] = mix == 0 ? dry : dry + static_cast<float>(mix) * (wet - dry);
        }
        sum[0][j] = sum[1][j] = weight[j] = 0;
        ++clock;
    }
private:
    int index(int64_t t) const noexcept { return static_cast<int>((t % capacity + capacity) % capacity); }
    double rate = 48000, maxPeriod = 750, predicted = -1, synthesis = 0, mix = 0, slew = 0;
    int delay = 4800, capacity = 1;
    int64_t clock = 0;
    size_t markCount = 0;
    bool refine = true;
    std::array<double, 256> marks{};
    TransformationHistory history;
    std::array<std::vector<float>, 2> sum;
    std::vector<float> weight;
};
}
