#pragma once
#include "PitchDetector.h"
namespace vocalpilot {
enum class TrackingState { unvoiced,acquiring,tracking,holding };
struct TrackedPitch {
    float midi=0,hz=0,reliability=0;
    bool valid=false;
    TrackingState state=TrackingState::unvoiced;
};
class TemporalPitchTracker {
public:
    void reset() noexcept { result={}; pending=0; pendingCount=misses=0; }
    TrackedPitch update(const PitchEstimate& raw) noexcept {
        const bool credible=raw.valid && raw.voiced && std::isfinite(raw.midi) && raw.confidence>=0.85f;
        if(!credible) {
            pendingCount=0; result.reliability=0;
            if(result.valid && ++misses<=2) result.state=TrackingState::holding;
            else { result={}; misses=0; }
            return result;
        }
        misses=0; const float delta=raw.midi-result.midi;
        if(!result.valid || std::abs(delta)>3.0f) {
            if(pendingCount==0 || std::abs(raw.midi-pending)>0.75f) { pending=raw.midi; pendingCount=1; }
            else { pending=raw.midi; ++pendingCount; }
            const int required=!result.valid?2:(std::abs(std::abs(delta)-12)<0.75f?4:3);
            result.reliability=0; result.state=result.valid?TrackingState::holding:TrackingState::acquiring;
            if(pendingCount<required) return result;
            result.midi=raw.midi; result.valid=true;
        } else result.midi+=(0.65f+0.25f*raw.confidence)*delta;
        pendingCount=0;
        result.hz=static_cast<float>(NoteTarget::frequency(result.midi));
        result.reliability=std::clamp((raw.confidence-0.80f)/0.15f,0.0f,1.0f);
        result.state=TrackingState::tracking;
        return result;
    }
private:
    TrackedPitch result;
    float pending=0;
    int pendingCount=0,misses=0;
};
}