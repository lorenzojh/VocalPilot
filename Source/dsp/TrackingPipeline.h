#pragma once
#include "TargetSelector.h"
#include "CorrectionTrajectory.h"
namespace vocalpilot {
struct Diagnostics {
    // First five fields retain the Milestone 1 API.
    float hz=0,midi=0,deviation=0,correction=0;
    int target=-1;
    float inputRms=0,confidence=0,trackedHz=0,trackedMidi=0,requested=0,wet=0,reliability=0;
    bool voiced=false,valid=false,trackedValid=false;
    TrackingState state=TrackingState::unvoiced;
};
// Shared sample-exact control path for plugin, offline CSV and tests.
class TrackingPipeline {
public:
    void prepare(double rate) noexcept { detector.prepare(rate); tracker.reset(); selector.reset(); trajectory.prepare(rate); raw={}; tracked={}; info={}; }
    bool push(float input,int key,bool minor,float strength,bool bypass) noexcept {
        const bool frame=detector.push(input);
        if(frame) {
            raw=detector.get(); tracked=tracker.update(raw);
            info.target=selector.update(tracked,key,minor);
            info.hz=raw.hz; info.midi=raw.midi; info.deviation=raw.valid?100*(raw.midi-std::round(raw.midi)):0;
            info.inputRms=raw.inputRms; info.confidence=raw.confidence; info.voiced=raw.voiced; info.valid=raw.valid;
            info.trackedHz=tracked.hz; info.trackedMidi=tracked.midi; info.trackedValid=tracked.valid;
            info.reliability=tracked.reliability; info.state=tracked.state;
        }
        // Scale changes invalidate the old target at the next analysis frame.
        const bool enabled=!bypass && raw.valid && tracked.valid && tracked.reliability>0 && info.target>=0 && strength>0;
        info.requested=enabled?static_cast<float>(std::clamp(100.0*(info.target-tracked.midi)*std::clamp(strength,0.0f,1.0f)*tracked.reliability,-1200.0,1200.0)):0;
        trajectory.push(info.requested,enabled);
        info.correction=bypass?0:trajectory.smoothedCents(); info.wet=trajectory.wetAmount();
        return frame;
    }
    Diagnostics diagnostics() const noexcept { return info; }
    double ratio() const noexcept { return trajectory.ratio(); }
    double hopSeconds() const noexcept { return detector.hopSeconds(); }
private:
    PitchDetector detector;
    TemporalPitchTracker tracker;
    TargetSelector selector;
    CorrectionTrajectory trajectory;
    PitchEstimate raw;
    TrackedPitch tracked;
    Diagnostics info;
};
}