#pragma once
#include "PitchDetector.h"
#include "PitchShifter.h"
#include "NoteTarget.h"

namespace vocalpilot {
struct Diagnostics { float hz = 0, midi = 0, deviation = 0, correction = 0; int target = -1; };
class CorrectionEngine {
public:
    void prepare(double rate) {
        detector.prepare(rate); shifter.prepare(rate);
        slew = 1 - std::exp(-1.0 / (0.025 * rate));
        mixSlew = 1 - std::exp(-1.0 / (0.005 * rate));
        cents = wet = 0; info = {};
    }
    int latencySamples() const noexcept { return shifter.latencySamples(); }
    Diagnostics diagnostics() const noexcept { return info; }
    void process(float* const* channels, int channelCount, int samples, int key, bool minor, float strength, bool bypass) noexcept {
        strength = std::clamp(strength, 0.0f, 1.0f);
        for (int i = 0; i < samples; ++i) {
            float input[2] {}, shifted[2] {}, dry[2] {};
            for (int ch = 0; ch < channelCount; ++ch)
                input[ch] = std::isfinite(channels[ch][i]) ? channels[ch][i] : 0;
            // Left input is the monophonic reference, avoiding anti-phase stereo cancellation.
            detector.push(input[0]);
            const auto estimate = detector.get();
            double requested = 0;
            if (estimate.hz > 0) {
                info.hz = estimate.hz;
                info.midi = static_cast<float>(NoteTarget::midi(estimate.hz));
                info.deviation = 100 * (info.midi - std::round(info.midi));
                info.target = NoteTarget::nearest(info.midi, key, minor);
                requested = std::clamp(100.0 * (info.target - info.midi) * strength, -1200.0, 1200.0);
            } else { info = {}; }
            if (bypass) requested = 0;
            cents += slew * (requested - cents);
            // Unity must use one fixed tap to avoid stationary two-tap comb filtering.
            const double desiredWet = !bypass && estimate.hz > 0 && strength > 0 && std::abs(cents) > 0.5 ? 1.0 : 0.0;
            wet += mixSlew * (desiredWet - wet);
            if (wet < 1e-6) wet = 0;
            shifter.process(input, shifted, dry, channelCount, std::exp2(cents / 1200));
            for (int ch = 0; ch < channelCount; ++ch)
                channels[ch][i] = dry[ch] + static_cast<float>(wet) * (shifted[ch] - dry[ch]);
            info.correction = bypass || estimate.hz == 0 ? 0 : static_cast<float>(cents);
        }
    }
private:
    PitchDetector detector;
    PitchShifter shifter;
    Diagnostics info;
    double slew = 0, mixSlew = 0, cents = 0, wet = 0;
};
}
