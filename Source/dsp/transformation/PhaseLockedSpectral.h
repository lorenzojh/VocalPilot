#pragma once
#include "Radix2FFT.h"
namespace vocalpilot {
// Peak-region frequency translation (not independent-bin phase propagation).
// Tracks peak instantaneous frequency, locks surrounding bin phases, and
// restores a pitch-adaptive cepstral envelope at the destination frequency.
class PhaseLockedSpectral final : public IPitchTransformationEngine {
public:
    void prepare(double sampleRate, int latency) override {
        rate = sampleRate; delay = latency; n = 256;
        while (n < rate * .060) n *= 2;
        hop = n / 8; bins = n / 2 + 1; capacity = 4 * delay + 2 * n;
        fft.prepare(n); history.prepare(capacity);
        for (auto& a : spectrum) a.assign(n, {});
        for (auto& a : result) a.assign(n, {});
        for (auto& a : sum) a.assign(capacity, 0);
        cepstrum.assign(n, {}); window.resize(n); weights.assign(capacity, 0);
        magnitude.assign(bins, 0); previousMagnitude.assign(bins, 0);
        envelope.assign(bins, 0); logMagnitude.assign(bins, 0);
        phase.assign(bins, 0); previousPhase.assign(bins, 0);
        synthesisPhase.assign(bins, 0); nextPhase.assign(bins, 0);
        peaks.assign(bins, 0); previousPeaks.assign(bins, 0);
        for (int i = 0; i < n; ++i) window[i] = .5 - .5 * std::cos(2 * transformPi * i / n);
        clock = 0; peakCount = 0; stage = 24; frames = 0; mix = 0;
        lateFrames = 0;
        slew = 1 - std::exp(-1 / (.003 * rate));
    }
    void process(const float* input, float* output, int channels, const TransformationControl& request) noexcept override {
        history.put(clock, input, channels, cleanControl(request));
        if (clock + 1 >= n && (clock + 1) % hop == 0) {
            frameStart = clock + 1 - n; frameControl = history.control(frameStart + n / 2);
            frameChannels = channels; stage = 0; nextStage = clock;
        }
        if (stage < 24 && clock >= nextStage) {
            renderStage(stage++); nextStage += std::max(1, hop / 25);
        }
        const auto c = history.control(clock - delay);
        const int j = index(clock);
        // Cold-start frames do not yet have a complete overlap lattice. A
        // transformed frame divided by a tiny edge window can amplify attacks.
        // Preserve aligned dry until full coverage, then use the normal fade.
        const bool covered = clock - delay >= n && weights[j] > 2.5;
        const bool active = c.voiced && std::abs(c.ratio - 1) > 1e-10 && covered;
        mix += slew * ((active ? c.amount : 0) - mix);
        if (mix < 1e-7) mix = 0;
        for (int ch = 0; ch < channels; ++ch) {
            const float dry = history.at(ch, clock - delay);
            const float wet = covered ? static_cast<float>(sum[ch][j] / weights[j]) : dry;
            output[ch] = mix == 0 ? dry : dry + static_cast<float>(mix) * (wet - dry);
        }
        sum[0][j] = sum[1][j] = weights[j] = 0;
        ++clock;
    }
    int fftSize() const noexcept { return n; }
    int lateFrameCount() const noexcept { return lateFrames; }
private:
    static double wrap(double x) noexcept { return x - 2 * transformPi * std::floor((x + transformPi) / (2 * transformPi)); }
    int index(int64_t t) const noexcept { return static_cast<int>((t % capacity + capacity) % capacity); }
    double envAt(double k) const noexcept {
        k = std::clamp(k, 0.0, static_cast<double>(bins - 1));
        const int i = static_cast<int>(k), j = std::min(i + 1, bins - 1);
        return envelope[i] + (k - i) * (envelope[j] - envelope[i]);
    }
    // The 100-ms output delay leaves at least one hop beyond the FFT
    // lookahead. Twenty-four scheduled pieces finish before the first output
    // sample is due. Scheduling is sample-clock based, never callback based.
    void renderStage(int task) noexcept {
        if (task < 2) {
            const int ch = task;
            if (ch < frameChannels) {
                for (int i=0;i<n;++i) spectrum[ch][i]=history.at(ch,frameStart+i)*window[i];
                fft.run(spectrum[ch]);
                std::fill(result[ch].begin(),result[ch].end(),Radix2FFT::Complex{});
            }
        } else if (task == 2) {
            double flux=0, energy=1e-12;
            for(int k=0;k<bins;++k) {
                magnitude[k]=std::abs(spectrum[0][k]); phase[k]=std::arg(spectrum[0][k]);
                flux+=std::max(0.0,magnitude[k]-previousMagnitude[k]); energy+=magnitude[k];
                envelope[k]=logMagnitude[k]=std::log(std::max(1e-9,magnitude[k]));
            }
            transient=flux/energy>.55;
            cutoff=std::clamp(static_cast<int>(rate/(2.5*std::max(65.0,frameControl.sourceHz))),8,n/8);
        } else if (task < 15) {
            const int part=(task-3)%4;
            if(part==0) for(int k=0;k<n;++k) { const int j=k<bins?k:n-k; cepstrum[k]=std::max(logMagnitude[j],envelope[j]); }
            else if(part==1) fft.run(cepstrum,true);
            else if(part==2) for(int k=1;k<n;++k) { const int q=std::min(k,n-k); cepstrum[k]*=q<=cutoff?(.54+.46*std::cos(transformPi*q/cutoff)):0; }
            else { fft.run(cepstrum); for(int k=0;k<bins;++k) envelope[k]=cepstrum[k].real(); }
        } else if (task == 15) {
            peakCount=0;
            for(int k=2;k<bins-2;++k)
                if(magnitude[k]>1e-7 && magnitude[k]>magnitude[k-1] && magnitude[k]>=magnitude[k+1]) peaks[peakCount++]=k;
        } else if (task < 20) {
            renderRegions((task-16)*peakCount/4,(task-15)*peakCount/4);
        } else if(task==20) {
            std::fill(previousPeaks.begin(),previousPeaks.end(),0);
            for(int i=0;i<peakCount;++i) { previousPeaks[peaks[i]]=1; synthesisPhase[peaks[i]]=nextPhase[peaks[i]]; }
            for(int k=0;k<bins;++k) { previousPhase[k]=phase[k]; previousMagnitude[k]=magnitude[k]; }
            ++frames;
        } else if(task<23) {
            const int ch=task-21;
            if(ch<frameChannels) {
                for(int k=1;k<n/2;++k) result[ch][n-k]=std::conj(result[ch][k]);
                fft.run(result[ch],true);
            }
        } else {
            if(frameStart+delay<clock) ++lateFrames;
            for(int i=0;i<n;++i) {
                const int j=index(frameStart+delay+i); weights[j]+=window[i]*window[i];
                for(int ch=0;ch<frameChannels;++ch) sum[ch][j]+=result[ch][i].real()*window[i];
            }
        }
    }
    void renderRegions(int firstPeak,int lastPeak) noexcept {
        const auto c=frameControl;
        for (int pIndex = firstPeak; pIndex < lastPeak; ++pIndex) {
            const int p = peaks[pIndex];
            const double omega = 2 * transformPi * p / n + wrap(phase[p] - previousPhase[p] - 2 * transformPi * p * hop / n) / hop;
            int predecessor = p, distance = 5;
            // Sorted peaks: inspect at most +/-4 bins in the previous map.
            for (int k = std::max(2, p - 4); k <= std::min(bins - 3, p + 4); ++k)
                if (previousPeaks[k] && std::abs(k - p) < distance) { predecessor = k; distance = std::abs(k - p); }
            nextPhase[p] = frames == 0 || transient || distance == 5 ? wrap(phase[p] + transformPi * p)
                : wrap(synthesisPhase[predecessor] + omega * c.ratio * hop);
            const double actualBin = omega * n / (2 * transformPi);
            const double delta = actualBin * (c.ratio - 1);
            const int low = pIndex == 0 ? 1 : (peaks[pIndex - 1] + p) / 2 + 1;
            const int high = pIndex + 1 == peakCount ? bins - 2 : (p + peaks[pIndex + 1]) / 2;
            const int first = std::max(1, static_cast<int>(std::ceil(low + delta)));
            const int last = std::min(bins - 2, static_cast<int>(std::floor(high + delta)));
            const auto rotation = std::polar(1.0, nextPhase[p] - phase[p] - transformPi * p);
            for (int d = first; d <= last; ++d) {
                const double source = d - delta;
                const int a = std::clamp(static_cast<int>(source), 0, bins - 2), b = a + 1;
                const double fraction = source - a;
                // Remove the window-centre phase ramp before interpolation.
                // Otherwise adjacent Hann-lobe bins cancel at half-bin shifts.
                const double gain = std::exp(std::clamp(envAt(d) - envAt(source), -2.3, 2.3));
                const double taper = std::clamp((bins - 1.0 - d) / 4, 0.0, 1.0);
                for (int ch = 0; ch < frameChannels; ++ch) {
                    auto value = (1 - fraction) * spectrum[ch][a] * (a % 2 ? -1.0 : 1.0)
                        + fraction * spectrum[ch][b] * (b % 2 ? -1.0 : 1.0);
                    result[ch][d] += value * rotation * (d % 2 ? -1.0 : 1.0) * gain * taper;
                }
            }
        }
    }
    double rate = 48000, mix = 0, slew = 0;
    int delay = 4800, n = 4096, bins = 2049, hop = 512, capacity = 1;
    int peakCount = 0, stage = 24, frameChannels = 2, cutoff = 0;
    int lateFrames = 0;
    int64_t frameStart = 0, nextStage = 0;
    TransformationControl frameControl;
    bool transient = false;
    int64_t clock = 0, frames = 0;
    Radix2FFT fft;
    TransformationHistory history;
    std::array<std::vector<Radix2FFT::Complex>, 2> spectrum, result;
    std::vector<Radix2FFT::Complex> cepstrum;
    std::array<std::vector<double>, 2> sum;
    std::vector<double> window, weights, magnitude, previousMagnitude, envelope, logMagnitude;
    std::vector<double> phase, previousPhase, synthesisPhase, nextPhase;
    std::vector<int> peaks, previousPeaks;
};
}
