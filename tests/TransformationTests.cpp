#include "dsp/transformation/TransformationBank.h"
#include <iostream>
#include <memory>
#include <random>
#include <limits>
using namespace vocalpilot;
void check(bool ok, const char* message) { if (!ok) { std::cerr << "FAIL " << message << '\n'; std::exit(1); } }
std::unique_ptr<IPitchTransformationEngine> make(int k) {
    if (k == 0) return std::make_unique<LegacyGranular>();
    if (k == 1) return std::make_unique<PitchSynchronous>();
    return std::make_unique<PhaseLockedSpectral>();
}
int main() {
    Radix2FFT fft; fft.prepare(1024); std::vector<Radix2FFT::Complex> values(1024);
    for (int i = 0; i < 1024; ++i) values[i] = std::sin(.12 * i) + .1 * std::cos(.391 * i);
    const auto original = values; fft.run(values); fft.run(values, true);
    for (int i = 0; i < 1024; ++i) check(std::abs(values[i] - original[i]) < 1e-12, "FFT inverse identity");
    for (double rate : {44100., 48000., 96000.}) for (int kind = 0; kind < 3; ++kind) {
        const int length = static_cast<int>(rate * .5), latency = hqLatency(rate);
        std::vector<float> input(length), reference(length);
        std::mt19937 random(17); for (auto& x : input) x = .1f * (static_cast<int>(random() % 20001) - 10000) / 10000;
        auto engine = make(kind); engine->prepare(rate, latency);
        for (int i = 0; i < length; ++i) {
            float x[]{input[i], -input[i]}, y[2]{};
            engine->process(x, y, 2, {220, 1, 1, 1, true});
            check(y[0] == (i < latency ? 0 : input[i - latency]), "sample exact zero shift");
            check(y[1] == -y[0], "unity stereo polarity");
        }
        engine->prepare(rate, latency);
        for (int i = 0; i < length; ++i) {
            float x[]{input[i], -input[i]}, y[2]{};
            engine->process(x, y, 2, {0, 1.5, 0, 1, false});
            check(y[0] == (i < latency ? 0 : input[i - latency]), "unvoiced exact aligned dry");
        }
        for (int partition : {1,7,31,32,63,64,127,128,255,256,511,512}) {
            engine->prepare(rate, latency);
            for (int start = 0; start < length; start += partition) for (int i = start; i < std::min(start + partition, length); ++i) {
                float x[]{static_cast<float>(.2 * std::sin(2 * transformPi * 220 * i / rate)), 0}, y[2]{}; x[1] = -x[0];
                const double cents = i < length / 2 ? 100 * std::sin(2 * transformPi * 5 * i / rate) : -700;
                engine->process(x, y, 2, {220, std::exp2(cents / 1200), 1, 1, true});
                check(std::isfinite(y[0]) && std::abs(y[0]) < 2, "finite bounded varying correction");
                check(std::abs(y[0] + y[1]) < 1e-6, "linked stereo phase");
                if (partition == 1) reference[i] = y[0]; else check(reference[i] == y[0], "block partition invariance");
            }
        }
        std::cout << "rate=" << rate << " engine=" << kind << " unity/unvoiced/stereo/12 partitions passed\n";
        if(kind==2) check(static_cast<PhaseLockedSpectral*>(engine.get())->lateFrameCount()==0,"spectral frames ready before output deadline");
    }
    TransformationBank bank; bank.prepare(48000);
    for (int i = 0; i < 48000; ++i) {
        float x[]{i % 29 == 0 ? std::numeric_limits<float>::infinity() : .1f, std::numeric_limits<float>::quiet_NaN()}, y[2]{};
        bank.process(x,y,2,{220, i%17==0?std::numeric_limits<double>::quiet_NaN():1.1,1,1,true},static_cast<TransformationKind>((i/7000)%3));
        check(std::isfinite(y[0]) && std::isfinite(y[1]), "pathological input and switching");
    }
    std::cout << "Transformation safety tests passed\n";
}
