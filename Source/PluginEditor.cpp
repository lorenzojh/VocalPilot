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
    setSize(500, 410); startTimerHz(15); timerCallback();
}
void VocalPilotEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff182129));
    g.setColour(juce::Colours::white); g.setFont(24.0f); g.drawText("VocalPilot", 20, 12, 300, 36, juce::Justification::left);
    g.setFont(13.0f); g.setColour(juce::Colour(0xffa8b5c0));
    g.drawText("Milestone 2  |  Vocal tracking diagnostics", 20, 48, 460, 24, juce::Justification::left);
    g.drawText("Minor = natural minor. Stereo detection uses left input.", 20, 376, 470, 22, juce::Justification::left);
}
void VocalPilotEditor::resized() {
    keyLabel.setBounds(20, 80, 55, 28); key.setBounds(75, 80, 110, 28);
    scaleLabel.setBounds(205, 80, 55, 28); scale.setBounds(260, 80, 120, 28); bypass.setBounds(390, 80, 95, 28);
    strengthLabel.setBounds(20, 122, 150, 28); strength.setBounds(170, 122, 310, 28);
    diagnostics.setBounds(20, 166, 460, 200);
}
void VocalPilotEditor::timerCallback() {
    const auto info = processor.readDiagnostics();
    static const char* states[] { "Unvoiced", "Acquiring", "Tracking", "Holding" };
    const juce::String raw=info.valid ? juce::String(info.hz,1)+" Hz / MIDI "+juce::String(info.midi,2)+" ("+noteName(static_cast<int>(std::round(info.midi)))+")" : "-- Hz / MIDI --";
    const juce::String tracked=info.trackedValid ? juce::String(info.trackedHz,1)+" Hz / MIDI "+juce::String(info.trackedMidi,2) : "-- Hz / MIDI --";
    const juce::String text="Raw: "+raw+"\nTracked: "+tracked
        +"\nConfidence: "+juce::String(info.confidence,2)+"  |  "+(info.voiced?"Voiced":"Unvoiced")
        +"  |  "+states[juce::jlimit(0,3,static_cast<int>(info.state))]
        +"\nInput RMS: "+juce::String(juce::Decibels::gainToDecibels(info.inputRms,-100.0f),1)+" dBFS"
        +"  |  Reliability: "+juce::String(info.reliability,2)
        +"\nTarget: "+(info.target>=0?noteName(info.target):"--")
        +"  |  Raw deviation: "+juce::String(info.deviation,1)+" cents"
        +"\nRequested: "+juce::String(info.requested,1)+" cents"
        +"\nSmoothed: "+juce::String(info.correction,1)+" cents  |  Shift mix: "+juce::String(info.wet*100,0)+"%";
    diagnostics.setText(text, juce::dontSendNotification);
}
