#pragma once
#include "TrackingPipeline.h"
#include "PitchShifter.h"
#include "transformation/TransformationBank.h"
#include "transformation/LivePitchSynchronous.h"
namespace vocalpilot {
class CorrectionEngine {
public:
    void prepare(double rate) { live=false; hq=false; pipeline.prepare(rate); shifter.prepare(rate); }
    void prepareHQ(double rate) { live=false; hq=true; pipeline.prepare(rate); bank.prepare(rate); }
    void prepareLive(double rate) { live=true; hq=false; liveLatency=static_cast<int>(std::ceil(rate*.018)); pipeline.prepare(rate); liveEngine.prepare(rate,liveLatency); }
    void selectTransformation(TransformationKind kind) noexcept { selected=kind; }
    int latencySamples() const noexcept { return live?liveLatency:hq?bank.latencySamples():shifter.latencySamples(); }
    Diagnostics diagnostics() const noexcept { return pipeline.diagnostics(); }
    void process(float* const* channels,int channelCount,int samples,int key,bool minor,float strength,bool bypass) noexcept {
        for(int i=0;i<samples;++i) {
            float input[2] {},shifted[2] {},dry[2] {};
            for(int ch=0;ch<channelCount;++ch) input[ch]=std::isfinite(channels[ch][i])?std::clamp(channels[ch][i],-16.0f,16.0f):0;
            pipeline.push(input[0],key,minor,strength,bypass);
            if(live || hq) {
                const auto d=pipeline.diagnostics();
                const TransformationControl control {d.trackedHz,pipeline.ratio(),d.confidence,d.wet,d.voiced && d.reliability>0};
                if(live) liveEngine.process(input,shifted,channelCount,control);
                else bank.process(input,shifted,channelCount,control,selected);
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
    LivePitchSynchronous liveEngine;
    bool live=false;
    int liveLatency=864;
    bool hq=false;
    TransformationKind selected=TransformationKind::legacy;
};
}
