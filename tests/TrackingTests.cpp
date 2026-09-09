#include "dsp/TrackingPipeline.h"
#include "dsp/CorrectionEngine.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <functional>
#include <cstdlib>
#include <cstdint>
using namespace vocalpilot;
namespace {
constexpr double pi=3.14159265358979323846;
int failures=0;
void check(bool ok,const std::string& message) { if(!ok) { std::cerr<<"FAIL: "<<message<<'\n'; ++failures; } }
struct Noise {
    uint32_t state=42;
    double next() { state=1664525u*state+1013904223u; return (static_cast<double>(state)/4294967296.0*2-1)*std::sqrt(3.0); }
};
double percentile(std::vector<double> values,double p) {
    if(values.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(values.begin(),values.end()); return values[static_cast<size_t>(std::ceil(p*(values.size()-1)))];
}
struct Metrics {
    std::vector<double> rawErrors,trackedErrors;
    int frames=0,voiced=0,tracked=0,octaves=0,gross=0,correctTargets=0,targetFrames=0,switches=0,trackedGross=0,trackedOctaves=0;
    double confidence=0;
    void add(const Diagnostics& d,double reference,int target) {
        ++frames; confidence+=d.confidence;
        if(d.valid) {
            ++voiced; const double e=std::abs(1200*std::log2(d.hz/reference)); rawErrors.push_back(e);
            if(std::abs(e-1200)<100) ++octaves; if(e>600) ++gross;
        }
        if(d.trackedValid && d.state==TrackingState::tracking) {
            ++tracked; const double e=std::abs(1200*std::log2(d.trackedHz/reference)); trackedErrors.push_back(e);
            if(e>600) ++trackedGross; if(std::abs(e-1200)<100) ++trackedOctaves;
        }
        if(target>=0) { ++targetFrames; if(d.target==target) ++correctTargets; }
    }
    void print(const std::string& name) const {
        std::cout<<"METRIC,"<<name<<','<<frames<<','<<voiced<<','<<tracked<<','
            <<percentile(rawErrors,.5)<<','<<percentile(rawErrors,.95)<<','<<percentile(rawErrors,1)<<','
            <<percentile(trackedErrors,.95)<<','<<octaves<<','<<gross<<','<<confidence/std::max(1,frames)<<','
            <<correctTargets<<','<<targetFrames<<','<<switches<<','<<percentile(trackedErrors,1)<<','<<trackedOctaves<<','<<trackedGross<<'\n';
    }
};
Metrics run(double rate,double seconds,const std::function<double(double)>& frequency,
            const std::vector<double>& harmonics,double amplitude,double noiseRms,int expectedTarget=-1) {
    TrackingPipeline pipeline; pipeline.prepare(rate); Noise noise;
    Metrics metrics; double phase=0; int previous=-1;
    for(int i=0;i<static_cast<int>(rate*seconds);++i) {
        const double t=i/rate,hz=frequency(t); phase+=2*pi*hz/rate;
        double sample=0; for(size_t h=0;h<harmonics.size();++h) sample+=harmonics[h]*std::sin((h+1)*phase);
        sample=amplitude*sample+noiseRms*noise.next();
        if(pipeline.push(static_cast<float>(sample),0,false,1,false) && t>=0.15) {
            const auto d=pipeline.diagnostics(); metrics.add(d,hz,expectedTarget);
            if(d.target>=0 && previous>=0 && d.target!=previous) ++metrics.switches;
            previous=d.target;
        }
    }
    return metrics;
}
void merge(Metrics& to,const Metrics& from) {
    to.rawErrors.insert(to.rawErrors.end(),from.rawErrors.begin(),from.rawErrors.end());
    to.trackedErrors.insert(to.trackedErrors.end(),from.trackedErrors.begin(),from.trackedErrors.end());
    to.frames+=from.frames; to.voiced+=from.voiced; to.tracked+=from.tracked; to.octaves+=from.octaves; to.gross+=from.gross;
    to.correctTargets+=from.correctTargets; to.targetFrames+=from.targetFrames; to.confidence+=from.confidence; to.switches+=from.switches;
    to.trackedGross+=from.trackedGross; to.trackedOctaves+=from.trackedOctaves;
}
PitchEstimate raw(float midi,float confidence=1) { PitchEstimate p; p.hz=static_cast<float>(NoteTarget::frequency(midi)); p.midi=midi; p.confidence=confidence; p.voiced=p.valid=true; return p; }
void stateTests() {
    TargetSelector target; target.reset();
    TrackedPitch p; p.valid=true; p.reliability=1; p.state=TrackingState::tracking;
    p.midi=60; check(target.update(p,0,false)==60,"initial C target");
    for(int i=0;i<100;++i) { p.midi=61+0.1f*std::sin(static_cast<float>(i)); check(target.update(p,0,false)==60,"C holds below forward threshold"); }
    p.midi=61.25f; check(target.update(p,0,false)==60,"one crossing frame insufficient"); check(target.update(p,0,false)==62,"forward boundary switch");
    for(int i=0;i<100;++i) { p.midi=61+0.1f*std::sin(static_cast<float>(i)); check(target.update(p,0,false)==62,"D holds above reverse threshold"); }
    p.midi=60.75f; target.update(p,0,false); check(target.update(p,0,false)==60,"reverse boundary switch");
    p.midi=61; check(target.update(p,2,false)==61,"key invalidates old target");
    p.midi=63; check(target.update(p,0,true)==63,"scale invalidates old target");
    TemporalPitchTracker tracker; tracker.reset();
    check(!tracker.update(raw(69)).valid,"attack needs two credible frames");
    check(tracker.update(raw(69)).valid,"attack acquired in second frame");
    for(float erroneous : {81.0f,57.0f,74.0f}) {
        const auto held=tracker.update(raw(erroneous)); check(held.midi==69 && held.reliability==0,"isolated jump suppressed");
        check(std::abs(tracker.update(raw(69)).midi-69)<0.01,"recovery after outlier");
    }
    for(int i=0;i<3;++i) check(tracker.update(raw(81)).midi==69,"octave requires sustained evidence");
    check(tracker.update(raw(81)).midi==81,"genuine octave accepted in four frames");
    auto held=tracker.update({}); check(held.valid && held.reliability==0,"unvoiced keeps memory but no correction support");
    tracker.update({}); check(!tracker.update({}).valid,"stale pitch expires after three missing frames");
    tracker.reset(); tracker.update(raw(69,.82f)); check(!tracker.update(raw(69,.82f)).valid,"low confidence cannot acquire");
    CorrectionTrajectory trajectory; trajectory.prepare(48000);
    float previous=0;
    for(int i=0;i<4800;++i) { trajectory.push(200,true); check(trajectory.smoothedCents()>=previous && trajectory.smoothedCents()-previous<0.2,"bounded monotonic trajectory"); previous=trajectory.smoothedCents(); }
    check(std::abs(previous-200)<4,"25ms one-pole reaches 98 percent by 100ms");
    std::cout<<"STATE,target hysteresis bidirectional and jitter; transient jumps; real octave; dropout; attack; smoothing,PASS\n";
}
void transitions() {
    for(double rate : {44100.,48000.,96000.}) for(auto pair : {std::pair<int,int>{60,62},{64,67},{57,69}}) {
        TrackingPipeline p; p.prepare(rate); double phase=0,firstRaw=-1,firstTarget=-1,changed=-1;
        for(int i=0;i<rate*1.0;++i) {
            const double t=i/rate,midi=t<0.4?pair.first:pair.second; phase+=2*pi*NoteTarget::frequency(midi)/rate;
            const float sample=static_cast<float>(.2*std::sin(phase)+.08*std::sin(2*phase)+.04*std::sin(3*phase));
            if(p.push(sample,0,false,1,false)) {
                const auto d=p.diagnostics();
                if(firstRaw<0 && d.valid) firstRaw=(i+1)/rate;
                if(firstTarget<0 && d.target==pair.first) firstTarget=(i+1)/rate;
                if(t>=.4 && d.target==pair.second && changed<0) changed=(i+1)/rate-.4;
            }
        }
        std::cout<<"TRANSITION,"<<rate<<','<<pair.first<<','<<pair.second<<','<<firstRaw*1000<<','<<firstTarget*1000<<','<<changed*1000<<'\n';
        check(firstRaw>0 && firstRaw<.080,"raw acquisition <80ms");
        check(firstTarget>0 && firstTarget<.100,"target acquisition <100ms");
        check(changed>0 && changed<.150,"discrete transition <150ms");
    }
}
void voicing() {
    TrackingPipeline p; p.prepare(48000); Noise n; double phase=0;
    int tp=0,fp=0,tn=0,fn=0; double release=-1,reacquire=-1;
    for(int i=0;i<48000*2;++i) {
        const double t=i/48000.0; const int section=static_cast<int>(t/.4);
        const bool voiced=section==1 || section==3;
        phase+=2*pi*220/48000;
        const float input=voiced?static_cast<float>(.2*std::sin(phase)+.08*std::sin(2*phase)):(section==2?static_cast<float>(.1*n.next()):0);
        if(p.push(input,0,false,1,false)) {
            const auto d=p.diagnostics();
            if(t>=.8 && !d.voiced && release<0) release=(i+1)/48000.0-.8;
            if(t>=1.2 && d.target==57 && d.state==TrackingState::tracking && reacquire<0) reacquire=(i+1)/48000.0-1.2;
            if(std::fmod(t,.4)>.080) {
                if(voiced) { if(d.voiced) ++tp; else ++fn; }
                else { if(d.voiced) ++fp; else ++tn; check(d.requested==0,"unvoiced requests no correction"); }
            }
        }
    }
    const double precision=double(tp)/std::max(1,tp+fp),recall=double(tp)/std::max(1,tp+fn);
    std::cout<<"VOICING,"<<tp<<','<<fp<<','<<tn<<','<<fn<<','<<precision<<','<<recall<<','<<release*1000<<','<<reacquire*1000<<'\n';
    check(precision>.95 && recall>.95,"voiced/unvoiced precision and recall");
    check(release>=0 && release<.080 && reacquire>=0 && reacquire<.120,"release and reacquisition bounds");
    for(double amplitude : {.0001,.0005,.004,.9}) {
        auto m=run(48000,.5,[](double){return 220.;},{1,.4,.2},amplitude,0,57);
        m.print("amplitude_"+std::to_string(amplitude));
        check(amplitude<.001?m.voiced==0:m.voiced==m.frames,"amplitude gate");
    }
    // High-pass noise is a crude sibilance proxy, not a recorded /s/ corpus.
    p.prepare(48000); double old=0; int voicedFrames=0,frames=0;
    for(int i=0;i<24000;++i) { double x=n.next(); float y=static_cast<float>(.1*(x-old)); old=x; if(p.push(y,0,false,1,false) && i>4800) {++frames; if(p.diagnostics().voiced) ++voicedFrames;} }
    std::cout<<"SIBILANCE_PROXY,"<<voicedFrames<<','<<frames<<'\n'; check(voicedFrames<frames*.05,"sibilance proxy rejection");
}
void pathological() {
    CorrectionEngine engine; engine.prepare(48000); float l[37] {},r[37] {}; float* channels[]{l,r};
    for(int block=0;block<2000;++block) {
        for(int i=0;i<37;++i) l[i]=r[i]=(block%3==0?std::numeric_limits<float>::quiet_NaN():block%3==1?std::numeric_limits<float>::infinity():std::numeric_limits<float>::max());
        engine.process(channels,2,1+block%37,block%12,block%2!=0,.5f,block%7==0);
        for(int i=0;i<1+block%37;++i) check(std::isfinite(l[i]) && std::isfinite(r[i]),"pathological output finite");
    }
    // Block partitioning must not change the sample-exact result.
    std::vector<float> a(24000),b; for(size_t i=0;i<a.size();++i) a[i]=static_cast<float>(.2*std::sin(2*pi*432*i/48000)); b=a;
    engine.prepare(48000); float* whole[]{a.data()}; engine.process(whole,1,static_cast<int>(a.size()),0,false,1,false);
    engine.prepare(48000); for(size_t offset=0;offset<b.size();) { const int count=std::min(static_cast<int>(b.size()-offset),1+static_cast<int>(offset%251)); float* part[]{b.data()+offset}; engine.process(part,1,count,0,false,1,false); offset+=count; }
    check(a==b,"block partition invariance");
}
}
int main() {
    std::cout<<std::fixed<<std::setprecision(4);
    std::cout<<"METRIC_HEADER,name,frames,voiced,tracked,raw_median_cents,raw_p95,raw_max,tracked_p95,octave_errors,gross_errors,mean_confidence,correct_targets,target_frames,switches,tracked_max,tracked_octaves,tracked_gross\n";
    for(const std::string kind : {"pure","harmonic","dominant_second","missing_fundamental"}) {
        Metrics all;
        const std::vector<double> harmonics=kind=="pure"?std::vector<double>{1}:kind=="harmonic"?std::vector<double>{1,.6,.3,.15}:kind=="dominant_second"?std::vector<double>{.08,1,.3,.2}:std::vector<double>{0,.8,.6,.3};
        for(double rate : {44100.,48000.,96000.}) for(double hz : {65.,70.,82.41,110.,146.83,220.,261.63,329.63,440.,659.25,880.,1000.}) {
            auto m=run(rate,.55,[hz](double){return hz;},harmonics,.2,0,NoteTarget::nearest(NoteTarget::midi(hz),0,false));
            check(m.voiced>=m.frames*.95 && m.tracked>=m.frames*.95,kind+" recall "+std::to_string(hz));
            check(percentile(m.rawErrors,.95)<20 && m.gross==0,kind+" accuracy "+std::to_string(hz)); merge(all,m);
        }
        all.print(kind); check(all.switches==0,kind+" stable target switches");
    }
    for(double snr : {30.,20.,10.,0.,-10.}) {
        Metrics all;
        for(double rate : {44100.,48000.,96000.}) for(double hz : {110.,220.,440.,880.}) {
            const double signalRms=.2*std::sqrt((1+.36+.09)/2);
            auto m=run(rate,.65,[hz](double){return hz;},{1,.6,.3},.2,signalRms/std::pow(10.,snr/20)); merge(all,m);
        }
        all.print("noise_snr_"+std::to_string(snr));
        if(snr>=10) check(all.voiced>=all.frames*.90 && percentile(all.rawErrors,.95)<25 && all.gross==0,"SNR>=10dB accuracy/recall");
        if(snr==0) check(all.trackedGross==0,"0dB noisy raw outlier suppressed by tracker");
        if(snr==-10) check(all.voiced<all.frames*.20,"very low SNR mostly rejected");
    }
    for(double rate : {44100.,48000.,96000.}) {
        auto vibrato=run(rate,2.,[](double t){return 440*std::exp2(.40*std::sin(2*pi*5*t)/12);},{1,.5,.2},.2,0,69);
        vibrato.print("vibrato_"+std::to_string(rate)); check(vibrato.switches==0 && vibrato.gross==0 && percentile(vibrato.trackedErrors,.95)<45,"vibrato trajectory/target");
        for(auto span : {std::pair<int,int>{60,67},{57,69}}) {
            auto slide=run(rate,2.,[span](double t){return NoteTarget::frequency(span.first+(span.second-span.first)*t/2);},{1,.4,.2},.2,0);
            slide.print("slide_"+std::to_string(span.first)+"_"+std::to_string(rate));
            check(slide.gross==0 && slide.tracked>=slide.frames*.9 && percentile(slide.trackedErrors,.95)<50,"slide continuity");
        }
    }
    stateTests(); transitions(); voicing(); pathological();
    std::cout<<"RESULT,"<<(failures==0?"PASS":"FAIL")<<","<<failures<<" failures\n";
    return failures==0?0:1;
}
