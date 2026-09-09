#pragma once
#include "TrackingPipeline.h"
#include "PitchShifter.h"
#include "transformation/TransformationBank.h"
namespace vocalpilot {
class CorrectionEngine {
public:
    void prepare(double rate) { hq=false; pipeline.prepare(rate); shifter.prepare(rate); }
    void prepareHQ(double rate) { hq=true; pipeline.prepare(rate); bank.prepare(rate); }
    void selectTransformation(TransformationKind kind) noexcept { selected=kind; }
    int latencySamples() const noexcept { return hq?bank.latencySamples():shifter.latencySamples(); }
    Diagnostics diagnostics() const noexcept { return pipeline.diagnostics(); }
    void process(float* const* channels,int channelCount,int samples,int key,bool minor,float strength,bool bypass) noexcept {
        for(int i=0;i<samples;++i) {
            float input[2] {},shifted[2] {},dry[2] {};
            for(int ch=0;ch<channelCount;++ch) input[ch]=std::isfinite(channels[ch][i])?std::clamp(channels[ch][i],-16.0f,16.0f):0;
            pipeline.push(input[0],key,minor,strength,bypass);
            if(hq) {
                const auto d=pipeline.diagnostics();
                bank.process(input,shifted,channelCount,{d.trackedHz,pipeline.ratio(),d.confidence,d.wet,d.voiced && d.reliability>0},selected);
                for(int ch=0;ch<channelCount;++ch) channels[ch][i]=shifted[ch];
                continue;
            }
            shifter.process(input,shifted,dry,channelCount,pipeline.ratio());
            const float wet=pipeline.diagnostics().wet;
            for(int ch=0;ch<channelCount;++ch) channels[ch][i]=dry[ch]+wet*(shifted[ch]-dry[ch]);
        }
    }
private:
    TrackingPipeline pipeline;
    PitchShifter shifter;
    TransformationBank bank;
    bool hq=false;
    TransformationKind selected=TransformationKind::legacy;
};
}
