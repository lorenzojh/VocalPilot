#include "PluginEditor.h"

namespace {
juce::String noteName(int midi) {
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[vocalpilot::NoteTarget::pitchClass(midi)]) + juce::String(static_cast<int>(std::floor(midi / 12.0)) - 1);
}
}
VocalPilotEditor::VocalPilotEditor(VocalPilotProcessor& p) : AudioProcessorEditor(p), processor(p),
    keyAttachment(p.parameters, "key", key), scaleAttachment(p.parameters, "scale", scale),
    strengthAttachment(p.parameters, "strength", strength), bypassAttachment(p.parameters, "bypass", bypass) {
    key.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scale.addItemList({ "Major", "Minor" }, 1);
    key.setSelectedItemIndex(static_cast<int>(p.parameters.getRawParameterValue("key")->load()), juce::dontSendNotification);
    scale.setSelectedItemIndex(static_cast<int>(p.parameters.getRawParameterValue("scale")->load()), juce::dontSendNotification);
    strength.setSliderStyle(juce::Slider::LinearHorizontal);
    strength.setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 25);
    strength.setTextValueSuffix(" %");
    keyLabel.setText("Key", juce::dontSendNotification); scaleLabel.setText("Scale", juce::dontSendNotification);
    strengthLabel.setText("Correction strength", juce::dontSendNotification);
    juce::Component* components[] { &key, &scale, &strength, &bypass, &keyLabel, &scaleLabel, &strengthLabel, &diagnostics };
    for (auto* component : components) addAndMakeVisible(component);
    diagnostics.setJustificationType(juce::Justification::topLeft);
    setSize(500, 340); startTimerHz(15); timerCallback();
}
void VocalPilotEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff182129));
    g.setColour(juce::Colours::white); g.setFont(24.0f); g.drawText("VocalPilot", 20, 12, 300, 36, juce::Justification::left);
    g.setFont(13.0f); g.setColour(juce::Colour(0xffa8b5c0));
    g.drawText("Milestone 1  |  Monophonic correction", 20, 48, 460, 24, juce::Justification::left);
    g.drawText("Minor = natural minor. Stereo detection uses left input.", 20, 306, 470, 22, juce::Justification::left);
}
void VocalPilotEditor::resized() {
    keyLabel.setBounds(20, 80, 55, 28); key.setBounds(75, 80, 110, 28);
    scaleLabel.setBounds(205, 80, 55, 28); scale.setBounds(260, 80, 120, 28); bypass.setBounds(390, 80, 95, 28);
    strengthLabel.setBounds(20, 122, 150, 28); strength.setBounds(170, 122, 310, 28);
    diagnostics.setBounds(20, 170, 460, 128);
}
void VocalPilotEditor::timerCallback() {
    const auto info = processor.readDiagnostics();
    juce::String text;
    if (info.hz > 0) text = "Detected: " + juce::String(info.hz, 1) + " Hz  |  " + noteName(static_cast<int>(std::round(info.midi)))
        + "\nMIDI: " + juce::String(info.midi, 2) + "  |  Deviation: " + juce::String(info.deviation, 1) + " cents"
        + "\nTarget: " + noteName(info.target) + "\nApplied correction: " + juce::String(info.correction, 1) + " cents";
    else text = "Detected: -- Hz  |  Waiting for voiced input\nMIDI: --  |  Deviation: --\nTarget: --\nApplied correction: 0 cents";
    diagnostics.setText(text, juce::dontSendNotification);
}
