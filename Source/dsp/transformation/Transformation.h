#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vocalpilot {
constexpr double transformPi = 3.14159265358979323846;
enum class TransformationKind { legacy, synchronous, spectral };
// Input-clock controls, independent of key/scale. ratio is already smoothed.
struct TransformationControl {
    double sourceHz = 0, ratio = 1;
    float confidence = 0, amount = 0;
    bool voiced = false;
};
inline TransformationControl cleanControl(TransformationControl c) noexcept {
    c.sourceHz = std::isfinite(c.sourceHz) ? std::clamp(c.sourceHz, 0.0, 1005.0) : 0;
    c.ratio = std::isfinite(c.ratio) ? std::clamp(c.ratio, 0.5, 2.0) : 1;
    c.amount = std::isfinite(c.amount) ? std::clamp(c.amount, 0.0f, 1.0f) : 0;
    c.voiced = c.voiced && c.sourceHz >= 64 && std::isfinite(c.confidence) && c.confidence >= .8f;
    return c;
}
class IPitchTransformationEngine {
public:
    virtual ~IPitchTransformationEngine() = default;
    virtual void prepare(double sampleRate, int latency) = 0;
    virtual void process(const float* input, float* output, int channels,
                         const TransformationControl& control) noexcept = 0;
};
// Absolute sample-clock ring. All storage allocated in prepare, never in process.
class TransformationHistory {
public:
    void prepare(int capacity) {
        size = capacity;
        for (auto& x : audio) x.assign(size, 0);
        controls.assign(size, {});
    }
    int index(int64_t t) const noexcept { return static_cast<int>((t % size + size) % size); }
    void put(int64_t t, const float* x, int channels, TransformationControl c) noexcept {
        const auto i = index(t);
        for (int ch = 0; ch < 2; ++ch) audio[ch][i] = ch < channels ? x[ch] : 0;
        controls[i] = c;
    }
    float at(int ch, int64_t t) const noexcept { return t < 0 ? 0 : audio[ch][index(t)]; }
    float at(int ch, double t) const noexcept {
        const auto i = static_cast<int64_t>(std::floor(t));
        const float f = static_cast<float>(t - i);
        return at(ch, i) + f * (at(ch, i + 1) - at(ch, i));
    }
    TransformationControl control(int64_t t) const noexcept { return t < 0 ? TransformationControl{} : controls[index(t)]; }
private:
    int size = 1;
    std::array<std::vector<float>, 2> audio;
    std::vector<TransformationControl> controls;
};
inline int hqLatency(double rate) noexcept {
    int n=256; while(n<rate*.060) n*=2;
    return std::max(static_cast<int>(std::ceil(rate*.100)),n+n/8);
}
}
