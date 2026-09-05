#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class VocalPilotEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VocalPilotEditor(VocalPilotProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    VocalPilotProcessor& processor;
    juce::ComboBox key, scale;
    juce::Slider strength;
    juce::ToggleButton bypass { "Bypass" };
    juce::Label keyLabel, scaleLabel, strengthLabel, diagnostics;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    ComboAttachment keyAttachment, scaleAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment strengthAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
};
