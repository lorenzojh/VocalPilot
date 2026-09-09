#include "dsp/transformation/TransformationBank.h"
#include "dsp/CorrectionEngine.h"
#include <chrono>
#include <iostream>
#include <memory>
using namespace vocalpilot;
int main() {
    std::cout << "engine,rate,block,mean_ms,p95_ms,p99_ms,max_ms,max_budget_percent\n";
    for (double rate : {44100.,48000.,96000.}) for (int block : {32,64,128,256,512}) for (int kind=0;kind<5;++kind) {
        LegacyGranular legacy; PitchSynchronous synchronous; PhaseLockedSpectral spectral; TransformationBank bank;
        CorrectionEngine core;
        IPitchTransformationEngine* engine=kind==0?static_cast<IPitchTransformationEngine*>(&legacy):kind==1?static_cast<IPitchTransformationEngine*>(&synchronous):&spectral;
        if(kind==4) { core.prepareHQ(rate); core.selectTransformation(TransformationKind::synchronous); }
        else if(kind==3) bank.prepare(rate); else engine->prepare(rate,hqLatency(rate));
        std::vector<double> times; times.reserve(static_cast<size_t>(rate*2/block)+1);
        float input[512][2],left[512],right[512]; double total=0;
        for(int t=0;t<rate*2;t+=block) {
            for(int i=0;i<block;++i) input[i][0]=input[i][1]=static_cast<float>(.2*std::sin(2*transformPi*130*(t+i)/rate)+.1*std::sin(2*transformPi*260*(t+i)/rate));
            for(int i=0;i<block;++i) left[i]=right[i]=input[i][0];
            const auto start=std::chrono::steady_clock::now();
            if(kind==4) { float* channels[]{left,right}; core.process(channels,2,block,0,false,1,false); }
            else for(int i=0;i<block;++i) { float out[2]; if(kind==3) bank.process(input[i],out,2,{130,1.07,1,1,true},TransformationKind::synchronous); else engine->process(input[i],out,2,{130,1.07,1,1,true}); }
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); times.push_back(ms); total+=ms;
        }
        std::sort(times.begin(),times.end());
        std::cout<<kind<<','<<rate<<','<<block<<','<<total/times.size()<<','<<times[static_cast<size_t>(.95*(times.size()-1))]<<','<<times[static_cast<size_t>(.99*(times.size()-1))]<<','<<times.back()<<','<<times.back()/(1000*block/rate)*100<<'\n';
    }
}
