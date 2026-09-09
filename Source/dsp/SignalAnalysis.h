#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace vocalpilot {
// Analysis-only DC rejection and fourth-order Butterworth low-pass.
class SignalAnalysis {
public:
    void prepare(double rate) noexcept {
        dcPole=std::exp(-2*pi*35/rate);
        const double omega=2*pi*std::min(2800.0,rate*0.2)/rate;
        for(int i=0;i<2;++i) {
            const double alpha=std::sin(omega)/(2*(i==0?0.5411961:1.3065630)), a0=1+alpha;
            auto& f=filters[i]; f.b0=(1-std::cos(omega))/(2*a0); f.b1=2*f.b0; f.b2=f.b0;
            f.a1=-2*std::cos(omega)/a0; f.a2=(1-alpha)/a0;
        }
        reset();
    }
    void reset() noexcept { previous=dc=energy=0; count=0; for(auto& f:filters) f.z1=f.z2=0; }
    float push(float input) noexcept {
        // Bound analysis arithmetic even for pathological finite input.
        double x=std::isfinite(input)?std::clamp(static_cast<double>(input),-16.0,16.0):0;
        dc=x-previous+dcPole*dc; previous=x; energy+=dc*dc; ++count; x=dc;
        for(auto& f:filters) { const double y=f.b0*x+f.z1; f.z1=f.b1*x-f.a1*y+f.z2; f.z2=f.b2*x-f.a2*y; x=y; }
        return static_cast<float>(x);
    }
    float takeRms() noexcept { const float result=static_cast<float>(std::sqrt(energy/std::max(1,count))); energy=0; count=0; return result; }
private:
    static constexpr double pi=3.14159265358979323846;
    struct Biquad { double b0=0,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0; };
    std::array<Biquad,2> filters {};
    double dcPole=0,previous=0,dc=0,energy=0;
    int count=0;
};
}