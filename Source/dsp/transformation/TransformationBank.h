#pragma once
#include "PitchSynchronous.h"
#include "PhaseLockedSpectral.h"
#include "../PitchShifter.h"
namespace vocalpilot {
class LegacyGranular final : public IPitchTransformationEngine {
public:
    void prepare(double rate, int latency) override {
        shifter.prepare(rate); extra = latency - shifter.latencySamples();
        history.prepare(latency * 2 + 32); clock = 0;
        mix=0; slew=1-std::exp(-1/(.003*rate));
    }
    void process(const float* input, float* output, int channels, const TransformationControl& request) noexcept override {
        const auto c = cleanControl(request);
        // Apply the input-clock trajectory to the legacy tap's nominal source
        // time. Its actual grain delays still vary, as in Milestone 1.
        const auto aligned = history.control(clock - shifter.latencySamples());
        float wet[2]{}, dry[2]{}, combined[2]{};
        shifter.process(input, wet, dry, channels, aligned.ratio);
        const float amount = aligned.voiced && std::abs(aligned.ratio - 1) > 1e-10 ? aligned.amount : 0;
        mix+=slew*(amount-mix); if(mix<1e-7) mix=0;
        for (int ch = 0; ch < channels; ++ch) combined[ch] = mix == 0 ? dry[ch] : dry[ch] + static_cast<float>(mix) * (wet[ch] - dry[ch]);
        history.put(clock, combined, channels, c);
        for (int ch = 0; ch < channels; ++ch) output[ch] = history.at(ch, clock - extra);
        ++clock;
    }
private:
    PitchShifter shifter;
    TransformationHistory history;
    int extra = 0;
    int64_t clock = 0;
    double mix=0, slew=0;
};
// All paths stay warm. A fixed common latency means engine selection never
// changes host delay compensation or clears an engine's phase/history.
class TransformationBank {
public:
    void prepare(double rate) {
        delay = hqLatency(rate); legacy.prepare(rate, delay); synchronous.prepare(rate, delay); spectral.prepare(rate, delay);
        blend = {1, 0, 0}; slew = 1 - std::exp(-1 / (.020 * rate));
    }
    int latencySamples() const noexcept { return delay; }
    void process(const float* input, float* output, int channels, TransformationControl control, TransformationKind kind) noexcept {
        float clean[2]{}, candidate[3][2]{};
        for (int ch = 0; ch < channels; ++ch) clean[ch] = std::isfinite(input[ch]) ? std::clamp(input[ch], -16.0f, 16.0f) : 0;
        legacy.process(clean, candidate[0], channels, control);
        synchronous.process(clean, candidate[1], channels, control);
        spectral.process(clean, candidate[2], channels, control);
        const int selected = std::clamp(static_cast<int>(kind), 0, 2);
        for (int k = 0; k < 3; ++k) { blend[k] += slew * ((selected == k ? 1 : 0) - blend[k]); if (blend[k] < 1e-9) blend[k] = 0; }
        const double total = blend[0] + blend[1] + blend[2];
        for (int ch = 0; ch < channels; ++ch) {
            // Preserve bit-identical dry samples when all paths agree.
            output[ch] = candidate[0][ch] == candidate[1][ch] && candidate[1][ch] == candidate[2][ch] ? candidate[0][ch]
                : static_cast<float>((blend[0] * candidate[0][ch] + blend[1] * candidate[1][ch] + blend[2] * candidate[2][ch]) / total);
            if (!std::isfinite(output[ch])) output[ch] = 0;
        }
    }
private:
    int delay = 4800;
    double slew = 0;
    std::array<double, 3> blend{};
    LegacyGranular legacy;
    PitchSynchronous synchronous;
    PhaseLockedSpectral spectral;
};
}
