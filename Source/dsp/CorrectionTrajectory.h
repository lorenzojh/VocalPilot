#pragma once
#include <algorithm>
#include <cmath>
namespace vocalpilot {
class CorrectionTrajectory {
public:
    void prepare(double rate) noexcept { slew=1-std::exp(-1/(0.025*rate)); mixSlew=1-std::exp(-1/(0.005*rate)); cents=wet=0; }
    void push(float requested,bool enabled) noexcept {
        cents+=slew*((enabled?requested:0)-cents);
        const double desired=enabled && std::abs(cents)>0.5?1:0;
        wet+=mixSlew*(desired-wet); if(wet<1e-6) wet=0;
    }
    float smoothedCents() const noexcept { return static_cast<float>(cents); }
    float wetAmount() const noexcept { return static_cast<float>(wet); }
    double ratio() const noexcept { return std::exp2(cents/1200); }
private:
    double slew=0,mixSlew=0,cents=0,wet=0;
};
}