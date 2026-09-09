#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/TrackingPipeline.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>

int main(int argc,char** argv) {
    if(argc<3 || argc>6) {
        std::cerr<<"Usage: vocalpilot_analyse input.wav output.csv [key 0..11] [major|minor] [strength 0..100]\n"; return 2;
    }
    try {
        int key=0; float strength=100; bool minor=false;
        if(argc>3) { size_t used=0; key=std::stoi(argv[3],&used); if(used!=std::string(argv[3]).size() || key<0 || key>11) throw std::runtime_error("Key must be 0..11"); }
        if(argc>4) { const std::string scale=argv[4]; if(scale!="major" && scale!="minor") throw std::runtime_error("Scale must be major or minor"); minor=scale=="minor"; }
        if(argc>5) { size_t used=0; strength=std::stof(argv[5],&used); if(used!=std::string(argv[5]).size() || !std::isfinite(strength) || strength<0 || strength>100) throw std::runtime_error("Strength must be 0..100"); }
        const auto inputPath=std::filesystem::u8path(argv[1]),outputPath=std::filesystem::u8path(argv[2]);
        if(std::filesystem::exists(outputPath) && std::filesystem::equivalent(inputPath,outputPath)) throw std::runtime_error("Input and output must be different files");
        juce::WavAudioFormat wav;
        const juce::File input(juce::String::fromUTF8(argv[1]));
        auto inputStream=input.createInputStream();
        if(!inputStream) throw std::runtime_error("Cannot open WAV input");
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(inputStream.release(),true));
        if(!reader) throw std::runtime_error("Cannot read WAV input");
        if(reader->sampleRate<8000 || reader->sampleRate>192000 || reader->numChannels<1) throw std::runtime_error("Supported WAV rates: 8..192 kHz, at least one channel");
        std::ofstream output(outputPath); if(!output) throw std::runtime_error("Cannot open CSV output");
        output<<"timeSeconds,inputRms,rawFrequency,rawMidi,confidence,voiced,valid,trackedFrequency,trackedMidi,trackedValid,targetMidi,targetNote,rawCorrectionCents,requestedCorrectionCents,smoothedCorrectionCents,reliability,trackingState,shiftMix\n"<<std::fixed<<std::setprecision(7);
        vocalpilot::TrackingPipeline pipeline; pipeline.prepare(reader->sampleRate);
        juce::AudioBuffer<float> buffer(1,2048); int frames=0;
        static const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        static const char* states[]{"unvoiced","acquiring","tracking","holding"};
        for(juce::int64 offset=0;offset<reader->lengthInSamples;offset+=2048) {
            const int count=static_cast<int>(std::min<juce::int64>(2048,reader->lengthInSamples-offset));
            if(!reader->read(&buffer,0,count,offset,true,false)) throw std::runtime_error("WAV read failed");
            for(int i=0;i<count;++i) if(pipeline.push(buffer.getSample(0,i),key,minor,strength/100,false)) {
                const auto d=pipeline.diagnostics(); ++frames;
                std::string note="";
                if(d.target>=0) note=std::string(names[vocalpilot::NoteTarget::pitchClass(d.target)])+std::to_string(d.target/12-1);
                const float rawCorrection=d.valid && d.target>=0?100*(d.target-d.midi):0;
                output<<(offset+i+1)/reader->sampleRate<<','<<d.inputRms<<','<<d.hz<<','<<d.midi<<','<<d.confidence<<','<<d.voiced<<','<<d.valid<<','
                      <<d.trackedHz<<','<<d.trackedMidi<<','<<d.trackedValid<<','<<d.target<<','<<note<<','<<rawCorrection<<','<<d.requested<<','
                      <<d.correction<<','<<d.reliability<<','<<states[static_cast<int>(d.state)]<<','<<d.wet<<'\n';
            }
        }
        output.flush(); if(!output) throw std::runtime_error("CSV write failed");
        std::cout<<frames<<" analysis frames written; "<<reader->sampleRate<<" Hz, left channel, no resampling\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
    return 0;
}
