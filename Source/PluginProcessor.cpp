#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout VocalPilotProcessor::makeParameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "key", 1 }, "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "scale", 1 }, "Scale",
        juce::StringArray { "Major", "Minor" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "strength", 1 }, "Correction Strength",
        juce::NormalisableRange<float>(0, 100, 0.1f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}
VocalPilotProcessor::VocalPilotProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "VocalPilotState", makeParameters()) {
    keyValue = parameters.getRawParameterValue("key"); scaleValue = parameters.getRawParameterValue("scale");
    strengthValue = parameters.getRawParameterValue("strength"); bypassValue = parameters.getRawParameterValue("bypass");
}
void VocalPilotProcessor::prepareToPlay(double rate, int) {
    engine.prepare(rate); setLatencySamples(engine.latencySamples());
    hz.store(0); midi.store(0); deviation.store(0); correction.store(0); target.store(-1);
}
bool VocalPilotProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto out = layout.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet() == out;
}
void VocalPilotProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, false); }
void VocalPilotProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }
void VocalPilotProcessor::process(juce::AudioBuffer<float>& buffer, bool hostBypass) {
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, buffer.getNumSamples());
    const int count = std::min(2, std::min(buffer.getNumChannels(), getTotalNumInputChannels()));
    if (count == 0) return;
    engine.process(buffer.getArrayOfWritePointers(), count, buffer.getNumSamples(),
        static_cast<int>(keyValue->load()), scaleValue->load() > 0.5f,
        strengthValue->load() / 100, hostBypass || bypassValue->load() > 0.5f);
    const auto info = engine.diagnostics();
    hz.store(info.hz, std::memory_order_relaxed); midi.store(info.midi, std::memory_order_relaxed);
    deviation.store(info.deviation, std::memory_order_relaxed); correction.store(info.correction, std::memory_order_relaxed);
    target.store(static_cast<float>(info.target), std::memory_order_relaxed);
}
vocalpilot::Diagnostics VocalPilotProcessor::readDiagnostics() const noexcept {
    return { hz.load(std::memory_order_relaxed), midi.load(std::memory_order_relaxed),
        deviation.load(std::memory_order_relaxed), correction.load(std::memory_order_relaxed),
        static_cast<int>(target.load(std::memory_order_relaxed)) };
}
juce::AudioProcessorParameter* VocalPilotProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
void VocalPilotProcessor::getStateInformation(juce::MemoryBlock& data) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, data);
}
void VocalPilotProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* VocalPilotProcessor::createEditor() { return new VocalPilotEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalPilotProcessor(); }
