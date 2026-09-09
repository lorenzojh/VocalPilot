#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/dsp/PitchDetector.h"
#include <iostream>
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) return 1;
    juce::AudioPluginFormatManager manager;
    auto* format = new juce::VST3PluginFormat(); manager.addFormat(format);
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile(descriptions, juce::String::fromUTF8(argv[1]));
    if (descriptions.isEmpty()) { std::cerr << "No VST3 found\n"; return 1; }
    juce::String error;
    auto plugin = manager.createPluginInstance(*descriptions[0], 48000, 256, error);
    if (!plugin) { std::cerr << error << '\n'; return 1; }
    plugin->enableAllBuses(); plugin->prepareToPlay(48000,256);
    std::cout << "Loaded binary: " << plugin->getName() << ", latency " << plugin->getLatencySamples() << '\n';
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());
    if (!editor) return 1;
    juce::AudioBuffer<float> buffer(2,256); juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    if (argc > 2) {
        auto stream = std::make_unique<juce::FileOutputStream>(juce::File(juce::String::fromUTF8(argv[2])));
        if (!stream->openedOk()) return 1;
        juce::WavAudioFormat wav;
        writer.reset(wav.createWriterFor(stream.get(),48000,2,24,{},0));
        if (!writer) return 1;
        stream.release();
    }
    juce::AudioProcessorParameter* engineParameter=nullptr;
    for(auto* parameter:plugin->getParameters()) if(parameter->getName(64)=="Transformation Engine") engineParameter=parameter;
    if(!engineParameter || plugin->getLatencySamples()!=4800) return 1;
    bool passed=true;
    for(int engine=0;engine<3;++engine) {
    plugin->releaseResources();
    engineParameter->setValueNotifyingHost(engine/2.0f);
    plugin->prepareToPlay(48000,256);
    vocalpilot::PitchDetector detector; detector.prepare(48000);
    for (int block=0;block<600;++block) {
        for (int i=0;i<256;++i) for (int ch=0;ch<2;++ch)
            buffer.setSample(ch,i,0.2f*std::sin(2*juce::MathConstants<double>::pi*432*(block*256+i)/48000));
        plugin->processBlock(buffer,midi);
        if (writer && !writer->writeFromAudioSampleBuffer(buffer,0,buffer.getNumSamples())) return 1;
        for (int i=0;i<256;++i) {
            if (!std::isfinite(buffer.getSample(0,i)) || std::abs(buffer.getSample(0,i))>0.3) {
                std::cerr<<"Engine "<<engine<<" unexpected sample "<<buffer.getSample(0,i)<<" at "<<block*256+i<<'\n'; return 1;
            }
            detector.push(buffer.getSample(0,i));
        }
    }
    const auto hz = detector.get().hz;
    std::cout << "VST3 engine " << engine << " output for 432 Hz: " << hz << " Hz\n";
    passed=passed && hz>0 && std::abs(1200*std::log2(hz/440))<20;
    }
    editor.reset(); plugin->releaseResources();
    return passed ? 0 : 1;
}
