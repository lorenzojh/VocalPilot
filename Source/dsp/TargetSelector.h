#pragma once
#include "TemporalPitchTracker.h"
namespace vocalpilot {
class TargetSelector {
public:
    void reset() noexcept { target=pending=-1; pendingCount=0; lastKey=-1; lastMinor=false; }
    int update(const TrackedPitch& pitch,int key,bool minor) noexcept {
        if(key!=lastKey || minor!=lastMinor) { target=pending=-1; pendingCount=0; lastKey=key; lastMinor=minor; }
        if(!pitch.valid) { target=pending=-1; pendingCount=0; return target; }
        if(pitch.state!=TrackingState::tracking || pitch.reliability<=0) { pendingCount=0; return target; }
        const int candidate=NoteTarget::nearest(pitch.midi,key,minor);
        if(target<0) { target=candidate; return target; }
        // 20 cents past the midpoint, then two consecutive analysis frames.
        const float oldDistance=std::abs(pitch.midi-target),newDistance=std::abs(pitch.midi-candidate);
        if(candidate==target || oldDistance-newDistance<0.40f) { pendingCount=0; return target; }
        if(candidate!=pending) { pending=candidate; pendingCount=1; } else ++pendingCount;
        if(pendingCount>=2) { target=candidate; pendingCount=0; }
        return target;
    }
private:
    int target=-1,pending=-1,pendingCount=0,lastKey=-1;
    bool lastMinor=false;
};
}