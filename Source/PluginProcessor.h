#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/CorrectionEngine.h"

class VocalPilotProcessor final : public juce::AudioProcessor {
public:
    VocalPilotProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VocalPilot"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    vocalpilot::Diagnostics readDiagnostics() const noexcept;
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void process(juce::AudioBuffer<float>&, bool);
    vocalpilot::CorrectionEngine engine;
    std::atomic<float>* keyValue = nullptr;
    std::atomic<float>* scaleValue = nullptr;
    std::atomic<float>* strengthValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;
    static_assert(std::atomic<float>::is_always_lock_free, "DSP meters require lock-free floats");
    std::atomic<float> hz { 0 }, midi { 0 }, deviation { 0 }, correction { 0 }, target { -1 };
    // Independent diagnostic fields; the UI may see adjacent blocks, never torn scalars.
    std::array<std::atomic<float>,11> trackingMeters {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalPilotProcessor)
};
