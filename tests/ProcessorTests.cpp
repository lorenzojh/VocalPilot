#include "PluginProcessor.h"
#include <iostream>
#include <cstdlib>
#include <chrono>
void require(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    VocalPilotProcessor processor;
    for (auto channels : { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() }) {
        auto layout = processor.getBusesLayout(); layout.inputBuses.set(0, channels); layout.outputBuses.set(0, channels);
        require(processor.setBusesLayout(layout), "mono/stereo layout failed");
        processor.prepareToPlay(48000, 256);
        require(processor.getLatencySamples() == 992, "latency");
        juce::AudioBuffer<float> buffer(channels.size(), 256); juce::MidiBuffer midi;
        double maximumMs = 0, totalMs = 0;
        for (int block = 0; block < 200; ++block) {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) for (int i = 0; i < 256; ++i)
                buffer.setSample(ch, i, 0.2f * std::sin(2.0 * 3.141592653589793 * 432 * (block*256+i) / 48000));
            const auto start = std::chrono::steady_clock::now();
            processor.processBlock(buffer, midi);
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count();
            maximumMs = std::max(maximumMs, milliseconds); totalMs += milliseconds;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) for (int i = 0; i < 256; ++i)
                require(std::isfinite(buffer.getSample(ch, i)), "non-finite processor output");
        }
        require(std::abs(processor.readDiagnostics().hz - 432) < 2, "processor pitch meter");
        require(processor.readDiagnostics().target == 69, "processor target");
        const auto diagnostics=processor.readDiagnostics();
        require(diagnostics.voiced && diagnostics.valid && diagnostics.trackedValid, "tracking flags exposed");
        require(diagnostics.confidence>0.95f && diagnostics.reliability>0.95f, "confidence meters exposed");
        require(std::abs(diagnostics.trackedHz-432)<2 && diagnostics.state==vocalpilot::TrackingState::tracking, "tracked pitch exposed");
        require(diagnostics.requested>25 && diagnostics.requested<35, "requested correction exposed");
        std::cout << channels.size() << " channels, 256 samples at 48k: mean " << totalMs / 200
                  << " ms, max " << maximumMs << " ms (5.333 ms deadline)\n";
        processor.processBlockBypassed(buffer, midi);
        require(processor.readDiagnostics().correction == 0, "host bypass meter");
        processor.releaseResources();
    }
    processor.parameters.getParameter("key")->setValueNotifyingHost(7.0f / 11);
    processor.parameters.getParameter("scale")->setValueNotifyingHost(1);
    processor.parameters.getParameter("strength")->setValueNotifyingHost(0.37f);
    processor.parameters.getParameter("bypass")->setValueNotifyingHost(1);
    juce::MemoryBlock state; processor.getStateInformation(state);
    VocalPilotProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    for (const auto* id : { "key", "scale", "strength", "bypass" })
        require(std::abs(processor.parameters.getRawParameterValue(id)->load() - restored.parameters.getRawParameterValue(id)->load()) < 0.001, "state roundtrip");
    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
    require(editor && editor->getWidth() == 500, "editor creation");
    if (argc > 1) {
        juce::FileOutputStream stream { juce::File(juce::String::fromUTF8(argv[1])) };
        require(stream.openedOk(), "snapshot output");
        require(juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), stream), "editor snapshot");
    }
    std::cout << "Processor, state, and editor tests passed\n";
}
