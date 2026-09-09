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
    for (auto& meter : trackingMeters) meter.store(0, std::memory_order_relaxed);
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
    const float values[] { info.inputRms, info.confidence, info.trackedHz, info.trackedMidi,
        info.requested, info.wet, info.reliability, info.voiced ? 1.0f : 0.0f,
        info.valid ? 1.0f : 0.0f, info.trackedValid ? 1.0f : 0.0f, static_cast<float>(info.state) };
    for (size_t i=0; i<trackingMeters.size(); ++i) trackingMeters[i].store(values[i], std::memory_order_relaxed);
}
vocalpilot::Diagnostics VocalPilotProcessor::readDiagnostics() const noexcept {
    vocalpilot::Diagnostics result { hz.load(std::memory_order_relaxed), midi.load(std::memory_order_relaxed),
        deviation.load(std::memory_order_relaxed), correction.load(std::memory_order_relaxed),
        static_cast<int>(target.load(std::memory_order_relaxed)) };
    float values[11];
    for (size_t i=0; i<trackingMeters.size(); ++i) values[i]=trackingMeters[i].load(std::memory_order_relaxed);
    result.inputRms=values[0]; result.confidence=values[1]; result.trackedHz=values[2]; result.trackedMidi=values[3];
    result.requested=values[4]; result.wet=values[5]; result.reliability=values[6];
    result.voiced=values[7]>0.5f; result.valid=values[8]>0.5f; result.trackedValid=values[9]>0.5f;
    result.state=static_cast<vocalpilot::TrackingState>(static_cast<int>(values[10]));
    return result;
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
