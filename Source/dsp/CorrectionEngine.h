#pragma once
#include "TrackingPipeline.h"
#include "PitchShifter.h"
namespace vocalpilot {
class CorrectionEngine {
public:
    void prepare(double rate) { pipeline.prepare(rate); shifter.prepare(rate); }
    int latencySamples() const noexcept { return shifter.latencySamples(); }
    Diagnostics diagnostics() const noexcept { return pipeline.diagnostics(); }
    void process(float* const* channels,int channelCount,int samples,int key,bool minor,float strength,bool bypass) noexcept {
        for(int i=0;i<samples;++i) {
            float input[2] {},shifted[2] {},dry[2] {};
            for(int ch=0;ch<channelCount;++ch) input[ch]=std::isfinite(channels[ch][i])?std::clamp(channels[ch][i],-16.0f,16.0f):0;
            pipeline.push(input[0],key,minor,strength,bypass);
            shifter.process(input,shifted,dry,channelCount,pipeline.ratio());
            const float wet=pipeline.diagnostics().wet;
            for(int ch=0;ch<channelCount;++ch) channels[ch][i]=dry[ch]+wet*(shifted[ch]-dry[ch]);
        }
    }
private:
    TrackingPipeline pipeline;
    PitchShifter shifter;
};
}
