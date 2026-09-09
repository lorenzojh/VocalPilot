#pragma once
#include "SignalAnalysis.h"
#include "NoteTarget.h"
#include <limits>
namespace vocalpilot {
struct PitchEstimate {
    float hz=0,confidence=0,midi=0,inputRms=0,analysisRms=0;
    bool voiced=false,valid=false;
};
// Confidence is min(CMNDF quality, normalized correlation), not a probability.
class PitchDetector {
public:
    void prepare(double sampleRate) noexcept {
        signal.prepare(sampleRate);
        decimation=std::max(1,static_cast<int>(std::round(sampleRate/12000.0))); rate=sampleRate/decimation;
        minLag=std::max(2,static_cast<int>(std::floor(rate/1000.0)));
        maxLag=std::min(500,static_cast<int>(std::ceil(rate/65.0)));
        window=std::min(1000,static_cast<int>(std::ceil(rate*0.030)));
        frame=window+maxLag+2; hop=std::max(1,static_cast<int>(std::round(rate*0.010)));
        reset();
    }
    void reset() noexcept { signal.reset(); ring.fill(0); write=filled=counter=hopCounter=0; estimate={}; }
    double hopSeconds() const noexcept { return hop/rate; }
    double windowSeconds() const noexcept { return frame/rate; }
    bool push(float sample) noexcept {
        const float filtered=signal.push(sample);
        if(++counter<decimation) return false;
        counter=0; ring[write]=filtered; write=(write+1)%frame; filled=std::min(frame,filled+1);
        if(++hopCounter<hop) return false;
        hopCounter=0; const float rms=signal.takeRms();
        if(filled<frame) { estimate={}; estimate.inputRms=rms; return false; }
        analyse(rms); return true;
    }
    PitchEstimate get() const noexcept { return estimate; }
private:
    SignalAnalysis signal;
    std::array<float,2048> ring {},data {};
    std::array<double,2049> energyPrefix {};
    std::array<double,503> difference {},correlation {};
    int write=0,filled=0,counter=0,hopCounter=0,decimation=4,hop=120;
    int minLag=12,maxLag=185,window=360,frame=547;
    double rate=12000;
    PitchEstimate estimate;
    double valleyDepth(int lag) const noexcept {
        const double a=difference[lag-1],b=difference[lag],c=difference[lag+1],denominator=a-2*b+c;
        return denominator>1e-12?std::max(0.0,b-(a-c)*(a-c)/(8*denominator)):b;
    }
    void analyse(float rms) noexcept {
        estimate={}; estimate.inputRms=rms; energyPrefix[0]=0;
        // Newest first: the comparison window includes the present.
        for(int i=0;i<frame;++i) {
            data[i]=ring[(write-1-i+2*frame)%frame];
            energyPrefix[i+1]=energyPrefix[i]+static_cast<double>(data[i])*data[i];
        }
        const double energy=energyPrefix[window];
        estimate.analysisRms=static_cast<float>(std::sqrt(energy/window));
        if(rms<0.001f || estimate.analysisRms<0.0005f) return;
        difference[0]=1; double cumulative=0;
        for(int lag=1;lag<=maxLag+1;++lag) {
            double sum=0;
            for(int i=0;i<window;++i) { const double delta=static_cast<double>(data[i])-data[i+lag]; sum+=delta*delta; }
            const double pairedEnergy=energy+energyPrefix[lag+window]-energyPrefix[lag];
            cumulative+=sum; difference[lag]=cumulative>1e-20?sum*lag/cumulative:1;
            correlation[lag]=pairedEnergy>1e-20?1-sum/pairedEnergy:0;
        }
        // Prefer the shortest near-global minimum over the first threshold crossing.
        double best=1;
        for(int lag=minLag;lag<=maxLag;++lag)
            if(difference[lag]<=difference[lag-1] && difference[lag]<difference[lag+1]) best=std::min(best,valleyDepth(lag));
        for(int lag=minLag;lag<=maxLag;++lag) {
            const double b=difference[lag];
            if(valleyDepth(lag)>best+0.05 || b>difference[lag-1] || b>=difference[lag+1]) continue;
            const double a=difference[lag-1],c=difference[lag+1],denominator=a-2*b+c;
            const double offset=std::abs(denominator)>1e-12?std::clamp(0.5*(a-c)/denominator,-0.5,0.5):0;
            const double hz=rate/(lag+offset);
            const double refinedCorrelation=correlation[lag]+0.5*offset*(correlation[lag+1]-correlation[lag-1])
                +0.5*offset*offset*(correlation[lag-1]-2*correlation[lag]+correlation[lag+1]);
            estimate.confidence=static_cast<float>(std::clamp(std::min(1-valleyDepth(lag),refinedCorrelation),0.0,1.0));
            const bool energyBalance=estimate.analysisRms>=0.15f*rms;
            // Small interpolation tolerance at the advertised 65..1000 Hz boundaries.
            estimate.valid=estimate.voiced=hz>=64.675 && hz<=1005 && estimate.confidence>=0.80f && energyBalance;
            if(estimate.valid) { estimate.hz=static_cast<float>(hz); estimate.midi=static_cast<float>(NoteTarget::midi(hz)); }
            return;
        }
    }
};
}
