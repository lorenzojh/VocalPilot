#include "dsp/CorrectionEngine.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>
using namespace vocalpilot;
void require(bool ok, const char* message) { if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
constexpr double pi = 3.14159265358979323846;
int main() {
    require(std::abs(NoteTarget::midi(440) - 69) < 1e-9, "A4 MIDI");
    require(NoteTarget::nearest(61, 0, false) == 60, "tie picks lower");
    require(NoteTarget::nearest(71.9, 0, false) == 72, "octave boundary");
    require(NoteTarget::nearest(64, 0, true) == 63, "natural minor");
    for (int key = 0; key < 12; ++key) for (bool minor : {false, true})
        for (double n = 24; n < 100; n += 0.125)
            require(NoteTarget::permitted(NoteTarget::nearest(n, key, minor), key, minor), "all targets permitted");
    for (double rate : {44100., 48000., 96000., 192000.}) {
        PitchDetector detector;
        for (double hz : {70., 110., 220., 432., 880.}) {
            detector.prepare(rate);
            for (int i = 0; i < rate * 0.4; ++i) {
                const double phase = 2 * pi * hz * i / rate;
                detector.push(static_cast<float>(0.2 * std::sin(phase) + 0.12 * std::sin(2 * phase) + 0.06 * std::sin(3 * phase)));
            }
            require(detector.get().hz > 0, "voiced detection");
            const double error = 1200 * std::log2(detector.get().hz / hz);
            std::cout << "rate=" << rate << " input=" << hz << " error cents=" << error << '\n';
            require(std::abs(error) < 12, "pitch accuracy <12 cents");
        }
        for (int i = 0; i < rate / 3; ++i) detector.push(0);
        require(detector.get().hz == 0, "silence clears stale pitch");
        CorrectionEngine engine; engine.prepare(rate);
        const int length = static_cast<int>(rate * 2);
        std::vector<float> left(length), right(length);
        for (int i = 0; i < length; ++i) { left[i] = static_cast<float>(0.2 * std::sin(2 * pi * 432 * i / rate)); right[i] = -left[i]; }
        auto original = left;
        float* channels[] { left.data(), right.data() };
        engine.process(channels, 2, length, 0, false, 1, true);
        const int delay = engine.latencySamples();
        for (int i = 0; i < length; ++i) {
            require(left[i] == (i < delay ? 0 : original[i-delay]), "bypass exact delayed audio");
            require(right[i] == -left[i], "stereo linked");
        }
        for (double inputHz : {432., 448.}) {
            engine.prepare(rate);
            for (int i = 0; i < length; ++i) left[i] = static_cast<float>(0.2 * std::sin(2*pi*inputHz*i/rate));
            // Variable host block sizes exercise streaming state.
            int offset = 0;
            while (offset < length) {
                int count = std::min(length-offset, 1 + (offset * 17 % 511));
                float* block[] {left.data()+offset}; engine.process(block, 1, count, 0, false, 1, false); offset += count;
            }
            PitchDetector outputDetector; outputDetector.prepare(rate);
            for (int i = length/2; i < length; ++i) {
                require(std::isfinite(left[i]) && std::abs(left[i]) < 0.3f, "bounded shifted output");
                outputDetector.push(left[i]);
            }
            const double outputHz = outputDetector.get().hz;
            std::cout << "correction " << inputHz << " -> " << outputHz << '\n';
            require(outputHz > 0 && std::abs(1200*std::log2(outputHz/440)) < 20, "end-to-end correction toward A4");
        }
    }
    PitchDetector noiseDetector; noiseDetector.prepare(48000);
    std::mt19937 random(42); std::uniform_real_distribution<float> noise(-0.2f, 0.2f);
    int voiced = 0, frames = 0;
    for (int i=0;i<48000;++i) if (noiseDetector.push(noise(random))) {++frames; if (noiseDetector.get().hz > 0) ++voiced;}
    require(voiced < frames/10, "noise rejection");
    // Strength and musical controls, plus a key change while audio is running.
    CorrectionEngine controls; controls.prepare(48000);
    std::vector<float> audio(48000);
    float* mono[] {audio.data()};
    auto tone = [&] (double hz) { for (int i=0;i<48000;++i) audio[i] = static_cast<float>(0.2*std::sin(2*pi*hz*i/48000)); };
    tone(432); controls.process(mono,1,48000,0,false,0.5f,false);
    require(std::abs(controls.diagnostics().correction - 15.9f) < 1, "half strength scales cents");
    tone(NoteTarget::frequency(61.2)); controls.process(mono,1,48000,0,false,1,false);
    require(controls.diagnostics().target == 62, "C major excludes C sharp");
    tone(NoteTarget::frequency(61.2)); controls.process(mono,1,48000,2,false,1,false);
    require(controls.diagnostics().target == 61, "D major permits C sharp");
    tone(NoteTarget::frequency(63.8)); controls.process(mono,1,48000,0,true,1,false);
    require(controls.diagnostics().target == 63, "C minor targets E flat below midpoint to F");
    controls.prepare(48000); tone(432); auto dryOriginal = audio;
    controls.process(mono,1,48000,0,false,0,false);
    for (int i=0;i<48000;++i) require(audio[i] == (i<controls.latencySamples()?0:dryOriginal[i-controls.latencySamples()]), "zero strength is delayed dry");
    audio[0] = std::numeric_limits<float>::quiet_NaN(); audio[1] = std::numeric_limits<float>::infinity();
    controls.process(mono,1,48000,0,false,1,false);
    for (auto sample:audio) require(std::isfinite(sample), "non-finite input cannot poison DSP");
    std::cout << "All DSP tests passed\n";
}
