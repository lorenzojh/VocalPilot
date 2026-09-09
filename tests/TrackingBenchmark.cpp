#include "dsp/CorrectionEngine.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
using namespace vocalpilot;
int main() {
    std::cout<<"sample_rate,block,blocks,mean_ms,p95_ms,max_ms,budget_ms,max_budget_percent\n"<<std::fixed<<std::setprecision(6);
    for(double rate : {44100.,48000.,96000.}) for(int block : {32,64,128,256,512}) {
        CorrectionEngine engine; engine.prepare(rate);
        const int length=static_cast<int>(rate*3); std::vector<float> left(length),right(length);
        for(int i=0;i<length;++i) { const double p=2*3.141592653589793*220*i/rate; left[i]=right[i]=static_cast<float>(.2*std::sin(p)+.1*std::sin(2*p)+.05*std::sin(3*p)); }
        std::vector<double> times; times.reserve(length/block+1);
        for(int offset=0;offset<length;offset+=block) {
            float* data[]{left.data()+offset,right.data()+offset}; const int count=std::min(block,length-offset);
            const auto begin=std::chrono::steady_clock::now(); engine.process(data,2,count,0,false,1,false);
            const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
            if(count==block) times.push_back(elapsed);
        }
        double total=0; for(auto t:times) total+=t; std::sort(times.begin(),times.end());
        const double budget=1000*block/rate;
        std::cout<<rate<<','<<block<<','<<times.size()<<','<<total/times.size()<<','<<times[static_cast<size_t>(.95*(times.size()-1))]<<','<<times.back()<<','<<budget<<','<<100*times.back()/budget<<'\n';
    }
}
