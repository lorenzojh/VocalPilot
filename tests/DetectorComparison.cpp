#include "PitchDetector.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>
// Include directory selects either the current DSP header or an extracted baseline.
// Same stationary fixtures as TrackingTests; valid-frame error excludes misses.
int main() {
    std::cout<<"type,frames,valid_frames,median_cents,p95_cents,gross_errors\n";
    for(int type=0;type<4;++type) {
        std::vector<double> errors; int frames=0,valid=0,gross=0;
        const std::vector<double> hs=type==0?std::vector<double>{1}:type==1?std::vector<double>{1,.6,.3,.15}:type==2?std::vector<double>{.08,1,.3,.2}:std::vector<double>{0,.8,.6,.3};
        for(double rate:{44100.,48000.,96000.}) for(double hz:{65.,70.,82.41,110.,146.83,220.,261.63,329.63,440.,659.25,880.,1000.}) {
            vocalpilot::PitchDetector d; d.prepare(rate); double phase=0;
            for(int i=0;i<rate*.55;++i) {
                phase+=2*3.141592653589793*hz/rate; double x=0;
                for(size_t h=0;h<hs.size();++h) x+=.2*hs[h]*std::sin((h+1)*phase);
                if(d.push(static_cast<float>(x)) && i/rate>=.15) {
                    ++frames; if(d.get().hz>0) { ++valid; const double e=std::abs(1200*std::log2(d.get().hz/hz)); errors.push_back(e); if(e>600)++gross; }
                }
            }
        }
        std::sort(errors.begin(),errors.end());
        if(errors.empty()) return 1;
        std::cout<<type<<','<<frames<<','<<valid<<','<<errors[static_cast<size_t>(std::ceil(.5*(errors.size()-1)))]<<','<<errors[static_cast<size_t>(std::ceil(.95*(errors.size()-1)))]<<','<<gross<<'\n';
    }
}
