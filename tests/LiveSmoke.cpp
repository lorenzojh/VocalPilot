#include "PluginProcessor.h"
#include <iostream>
#include <chrono>
#include <cstdlib>
using namespace vocalpilot;
void check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
std::vector<float> render(double f,double cents,int marker=-1,bool zero=false){
    LivePitchSynchronous e;e.prepare(48000,864);std::vector<float> y(49024);int gaps=0;
    for(int start=0;start<int(y.size());start+=128)for(int i=start;i<std::min(start+128,int(y.size()));++i){
        double p=2*transformPi*f*i/48000;
        float x[2]{float(.2*std::sin(p)+.12*std::sin(2*p)+.06*std::sin(3*p)),0},out[2]{};
        if(i==marker)x[0]+=.0001f;
        e.process(x,out,1,{f,zero?1:std::exp2(cents/1200),1,1,true});y[i]=out[0];
        check(std::isfinite(out[0]),"Live nonfinite");
        if(i>12000 && i<47000 && f>=140 && f<=500)gaps+=!e.covered;
    }
    if(!zero && f>=140 && f<=500)check(gaps==0,"Live steady corrected coverage gap");return y;
}
int main(){juce::ScopedJuceInitialiser_GUI init;
    for(double f:{140.,180.,220.,350.,500.})for(double cents:{25.,100.,-100.,200.,-200.}){
        if(std::abs(cents)==200 && f!=140 && f!=500)continue;
        auto base=render(f,cents);PitchDetector detector;detector.prepare(48000);
        for(float v:base)detector.push(v);
        double err=1200*std::log2(detector.get().hz/(f*std::exp2(cents/1200)));
        check(std::isfinite(err)&&std::abs(err)<5,"Live pitch error");
        int first=99999,last=0;
        for(int marker:{16003,17003,18003}){auto marked=render(f,cents,marker);
            for(int i=0;i<int(base.size());++i)if(std::abs(marked[i]-base[i])>1e-7){first=std::min(first,i-marker);last=std::max(last,i-marker);}}
        check(last<=864 && first>0,"Live marker exceeds host budget");
        std::cout<<"Live "<<f<<" Hz "<<cents<<" cents: error "<<err<<" cents; marker "<<first<<".."<<last<<" samples\n";
    }
    for(double f:{100.,220.,550.}){
        auto y=render(f,100,-1,f==220);
        for(int i=0;i<48000;++i){double p=2*transformPi*f*i/48000;float expected=float(.2*std::sin(p)+.12*std::sin(2*p)+.06*std::sin(3*p));
            check(y[i+864]==expected,"zero correction / unsupported dry transparency");}
    }
    VocalPilotProcessor p;p.parameters.getParameter("mode")->setValueNotifyingHost(1);p.prepareToPlay(48000,128);
    check(p.getLatencySamples()==864&&p.isLiveMode(),"Live host latency");
    juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;PitchDetector detector;detector.prepare(48000);double cpu=0;
    for(int block=0;block<750;++block){for(int i=0;i<128;++i)for(int ch=0;ch<2;++ch)b.setSample(ch,i,float(.2*std::sin(2*transformPi*432*(block*128+i)/48000)));
        auto t=std::chrono::steady_clock::now();p.processBlock(b,midi);cpu+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
        for(int i=0;i<128;++i){check(std::isfinite(b.getSample(0,i))&&b.getSample(0,i)==b.getSample(1,i),"Live stereo/finite");detector.push(b.getSample(0,i));}}
    check(std::abs(1200*std::log2(detector.get().hz/440))<5,"Live + M2 did not correct 432 to 440");
    std::cout<<"M2 + Live output "<<detector.get().hz<<" Hz; mean stereo 128-block CPU "<<cpu/750<<" ms\n";
    for(double edge:{140.,500.}){
        p.prepareToPlay(48000,128);detector.prepare(48000);
        for(int block=0;block<750;++block){for(int i=0;i<128;++i)for(int ch=0;ch<2;++ch)b.setSample(ch,i,float(.2*std::sin(2*transformPi*edge*(block*128+i)/48000)));
            p.processBlock(b,midi);for(int i=0;i<128;++i)detector.push(b.getSample(0,i));}
        const auto info=p.readDiagnostics();const double targetHz=440*std::exp2((info.target-69)/12.0);
        std::cout<<"M2 range edge "<<edge<<" tracked "<<info.trackedHz<<" output "<<detector.get().hz<<" target "<<targetHz<<'\n';
        check(std::abs(1200*std::log2(detector.get().hz/targetHz))<5,"M2 range edge correction");
    }
    juce::MemoryBlock state;p.getStateInformation(state);VocalPilotProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));restored.prepareToPlay(48000,128);
    check(restored.isLiveMode()&&restored.getLatencySamples()==864,"Live state roundtrip");
    p.parameters.getParameter("mode")->setValueNotifyingHost(0);p.prepareToPlay(48000,128);check(!p.isLiveMode()&&p.getLatencySamples()==4800,"HQ return");
    // Old sessions omit mode, even when restored over a currently Live instance.
    auto old=restored.parameters.copyState();for(int i=old.getNumChildren()-1;i>=0;--i)if(old.getChild(i).getProperty("id").toString()=="mode")old.removeChild(i,nullptr);
    juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*old.createXml(),legacy);restored.setStateInformation(legacy.getData(),int(legacy.getSize()));restored.prepareToPlay(48000,128);
    check(!restored.isLiveMode(),"Old session must default HQ");
    p.parameters.getParameter("mode")->setValueNotifyingHost(1);
    juce::MessageManager::callAsync([&]{check(p.isLiveMode()&&p.getLatencySamples()==864,"Message-thread mode switch");juce::MessageManager::getInstance()->stopDispatchLoop();});
    juce::MessageManager::getInstance()->runDispatchLoop();
    std::cout<<"Live smoke passed\n";
}
