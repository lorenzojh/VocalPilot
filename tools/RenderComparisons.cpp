#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/TrackingPipeline.h"
#include "dsp/transformation/TransformationBank.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace vocalpilot;
int main(int argc, char** argv) {
    try {
        if (argc < 3) throw std::runtime_error("Usage: vocalpilot_render input.wav NEW-output-directory [--key 0..11] [--minor] [--strength 0..100] [--cents -1200..1200 --f0 65..1000] [--trajectory controls.csv] [--no-refine]");
        double cents = 0, f0 = 0, strength = 100; int key = 0; bool minor = false, refine = true, hasCents = false;
        std::string trajectory;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--minor") minor = true;
            else if (arg == "--no-refine") refine = false;
            else {
                if (i + 1 == argc) throw std::runtime_error("Missing option value");
                const std::string value = argv[++i];
                if (arg == "--trajectory") trajectory = value;
                else {
                    size_t consumed = 0; const double v = std::stod(value, &consumed);
                    if (!std::isfinite(v) || consumed != value.size()) throw std::runtime_error("Invalid numeric option");
                    if (arg == "--key") { if (v != std::floor(v) || v < 0 || v > 11) throw std::runtime_error("Invalid key"); key = static_cast<int>(v); }
                    else if (arg == "--strength") strength = v;
                    else if (arg == "--cents") { cents = v; hasCents = true; }
                    else if (arg == "--f0") f0 = v;
                    else throw std::runtime_error("Unknown option");
                }
            }
        }
        if (std::abs(cents) > 1200 || strength < 0 || strength > 100 || (f0 != 0 && (f0 < 65 || f0 > 1000)) || (hasCents && f0 == 0))
            throw std::runtime_error("Options outside supported range; fixed cents requires source F0");
        if(!trajectory.empty() && (hasCents || f0>0)) throw std::runtime_error("Choose supplied trajectory or fixed F0/cents, not both");
        juce::File input = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[1]));
        juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[2]));
        if (folder.exists()) throw std::runtime_error("Output directory already exists; use a new directory");
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(input));
        if (!reader || reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * 600)
            throw std::runtime_error("Need readable audio, 8..192 kHz, 0..600 seconds");
        const auto rate = reader->sampleRate;
        const int length = static_cast<int>(reader->lengthInSamples), channels = std::min(2, static_cast<int>(reader->numChannels));
        juce::AudioBuffer<float> source(channels, length);
        if (!reader->read(&source, 0, length, 0, true, channels == 2)) throw std::runtime_error("Audio read failed");
        std::vector<TransformationControl> controls(length);
        TrackingPipeline pipeline; pipeline.prepare(rate);
        for (int i = 0; i < length; ++i) {
            for (int ch = 0; ch < channels; ++ch) {
                auto& v = source.getWritePointer(ch)[i]; v = std::isfinite(v) ? std::clamp(v, -16.0f, 16.0f) : 0;
            }
            pipeline.push(source.getSample(0, i), key, minor, static_cast<float>(strength / 100), false);
            const auto d = pipeline.diagnostics();
            controls[i] = f0 > 0 ? TransformationControl{f0, std::exp2(cents / 1200), 1, 1, true}
                : TransformationControl{d.trackedHz, pipeline.ratio(), d.confidence, d.wet, d.voiced && d.reliability > 0};
        }
        if (!trajectory.empty()) {
            std::ifstream in(trajectory); std::string line;
            if (!std::getline(in, line)) throw std::runtime_error("Missing trajectory header");
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line != "sample,sourceHz,ratio,voiced,confidence,amount") throw std::runtime_error("Invalid trajectory header");
            for (int i = 0; i < length; ++i) {
                if (!std::getline(in, line)) throw std::runtime_error("Trajectory shorter than input");
                std::replace(line.begin(), line.end(), ',', ' '); std::istringstream row(line);
                int sample, voiced; TransformationControl c;
                if (!(row >> sample >> c.sourceHz >> c.ratio >> voiced >> c.confidence >> c.amount) || sample != i || (voiced != 0 && voiced != 1)) throw std::runtime_error("Invalid trajectory row");
                std::string extra; if (row >> extra) throw std::runtime_error("Extra trajectory fields");
                if (!std::isfinite(c.sourceHz) || !std::isfinite(c.ratio) || !std::isfinite(c.confidence) || !std::isfinite(c.amount) || c.ratio < .5 || c.ratio > 2 || c.sourceHz < 0 || c.sourceHz > 1005 || (voiced && c.sourceHz < 64) || c.confidence < 0 || c.confidence > 1 || c.amount < 0 || c.amount > 1) throw std::runtime_error("Invalid trajectory values");
                c.voiced = voiced != 0; controls[i] = c;
            }
            while (std::getline(in, line)) if (!line.empty()) throw std::runtime_error("Trajectory longer than input");
        }
        if (!folder.createDirectory()) throw std::runtime_error("Cannot create output directory");
        std::ofstream manifest(folder.getChildFile("render.json").getFullPathName().toStdString());
        manifest << "{\"sample_rate\":" << rate << ",\"samples\":" << length << ",\"channels\":" << channels
                 << ",\"latency_samples_removed\":" << hqLatency(rate) << ",\"epoch_refinement\":" << (refine ? "true" : "false")
                 << ",\"trajectory\":\"" << (!trajectory.empty() ? "supplied" : f0 > 0 ? "fixed_oracle" : "milestone2_causal") << "\"}\n";
        std::ofstream controlFile(folder.getChildFile("controls.csv").getFullPathName().toStdString());
        controlFile << "sample,sourceHz,ratio,voiced,confidence,amount\n" << std::setprecision(12);
        for (int i = 0; i < length; ++i) { const auto c = controls[i]; controlFile << i << ',' << c.sourceHz << ',' << c.ratio << ',' << c.voiced << ',' << c.confidence << ',' << c.amount << '\n'; }
        LegacyGranular legacy; PitchSynchronous synchronous; PhaseLockedSpectral spectral;
        const char* names[]{"dry", "legacy", "synchronous", "spectral"};
        IPitchTransformationEngine* engines[]{nullptr, &legacy, &synchronous, &spectral};
        const int latency = hqLatency(rate);
        for (int k = 0; k < 4; ++k) {
            juce::AudioBuffer<float> rendered(channels, length); rendered.clear();
            if (k == 0) rendered.makeCopyOf(source);
            else {
                engines[k]->prepare(rate, latency); synchronous.setEpochRefinement(refine);
                for (int i = 0; i < length + latency; ++i) {
                    float x[2]{}, y[2]{};
                    if (i < length) for (int ch = 0; ch < channels; ++ch) x[ch] = source.getSample(ch, i);
                    engines[k]->process(x, y, channels, controls[std::min(i, length - 1)]);
                    if (i >= latency) for (int ch = 0; ch < channels; ++ch) {
                        if (!std::isfinite(y[ch])) throw std::runtime_error("Non-finite render");
                        if (std::abs(y[ch]) >= 1) throw std::runtime_error("Render would clip PCM; lower input gain");
                        rendered.setSample(ch, i - latency, y[ch]);
                    }
                }
            }
            juce::WavAudioFormat wav;
            auto stream = folder.getChildFile(juce::String(names[k]) + ".wav").createOutputStream();
            if (!stream) throw std::runtime_error("Cannot create WAV");
            auto* rawStream = stream.release();
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(rawStream, rate, channels, 24, {}, 0));
            if (!writer) { delete rawStream; throw std::runtime_error("Cannot create WAV writer"); }
            if (!writer->writeFromAudioSampleBuffer(rendered, 0, length)) throw std::runtime_error("WAV write failed");
        }
        if (!manifest || !controlFile) throw std::runtime_error("Metadata write failed");
        std::cout << "Four aligned renders; identical " << length << "-sample trajectory; removed " << latency << " samples\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
