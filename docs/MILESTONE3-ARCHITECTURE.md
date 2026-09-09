# Milestone 3 transformation architecture

M2's detector, confidence, tracker, target selection and fixed 25 ms trajectory
remain the control source. Their headers are unchanged. The M1 shifter is also
unchanged in `PitchShifter.h`. `CorrectionEngine::prepare` retains the original
path for historical regressions; the VST uses `prepareHQ` and the new bank.

```
left audio -> unchanged TrackingPipeline -> source F0, ratio, voicing, mix
stereo audio + input-clock controls -> TransformationBank
   LegacyGranular | PitchSynchronous | PhaseLockedSpectral
               fixed common delay -> 20 ms selection fade -> output
```

## Contract and timing

`IPitchTransformationEngine` accepts two-channel samples and a
`TransformationControl`: sourceHz, requested instantaneous ratio, confidence,
voiced state and wet amount. Musical keys and note names do not belong here.
Ratio is constrained to 0.5–2, supported source F0 to approximately 65–1000 Hz.
Target F0 is sourceHz * ratio. Note changes are expressed through this trajectory;
they do not reset the engine. Controls are timestamped on the input sample clock
and stored with the audio. This preserves the M2 *causal* control lag; it does not
pretend that M2's estimate describes an exactly identified historical epoch.

`prepare` allocates audio/control/OLA rings, FFT twiddles and permutation tables.
`process` advances exactly one sample regardless of host callback partition.
The bank sanitizes audio (non-finite -> zero, finite bounded to +/-16) before any
candidate sees it. No allocation, I/O, locks, threads, strings or UI operations
are used in the processing path. Individual engines expect bank-sanitized audio.

N is the smallest power of two >= 60 ms, with minimum 256. H=N/8. Common delay is
`max(ceil(0.100*rate), N+H)`: 4608 samples at 44.1 kHz (104.4898 ms), 4800 at 48 kHz
(100 ms), 9600 at 96 kHz (100 ms). The extra hop allows spectral calculations to
finish before their output is due. Each path has the same dry delay. All engines
stay warm, so selecting another engine neither reallocates nor changes host
latency. A 20 ms one-pole crossfade changes engine weights. Waveform phase can
differ between engines; fixed time alignment does not guarantee phase identity
or perceptual transparency during that diagnostic switch.

## LegacyGranular: reference, not a new algorithm

The unchanged two moving fractional-delay taps retain their raised-cosine
windows and +/-one-octave ratio range. The adapter adds delay to match the bank
and reads controls at the tap's nominal source time. Actual grain delay still
varies, so transient alignment while shifting remains approximate. Static
zero-shift uses exact delayed dry. The original M1/M2 engine path remains available
to its existing regression tests. No formant preservation is added to this reference.

## PitchSynchronous: original TD-PSOLA candidate

Tracked F0 predicts a period. After 0.25 period of lookahead, the engine searches
within +/-0.22 period for a positive waveform peak and refines it with a bounded
parabolic interpolation. The next prediction starts one tracked period later.
These are waveform synchronization marks, not proven glottal closure instants.
Breathy/diplophonic speech or dominant harmonics can confuse this estimator.
The `--no-refine` renderer option provides an ablation using predicted marks.

A separate synthesis clock advances by sourcePeriod/ratio. After three maximum
source periods of lookahead, the nearest valid analysis epoch supplies a Hann
window of two **source** periods. Audio inside the window is not resampled: its
local resonance frequencies remain fixed. Windows are added at synthesis epochs;
the source-time center advances normally, preserving duration. Both channels
use the left-channel epochs and identical windows. Fractional source sampling
uses linear interpolation; this is a limitation at very high frequencies.

Per-grain gain is 1/sqrt(ratio), an excitation-energy heuristic, not a loudness
guarantee. Do not divide by the instantaneous window sum: that can erase the
new periodic amplitude structure, particularly at an octave down. This failure
was found in the first sweep and fixed. Two-period support covers ratios down
to 0.5; wider intervals are explicitly outside the contract.

Formant preservation here comes from retaining waveform time scale within each
pitch-synchronous window. There is no LPC filter and no claim of explicit,
perfect source/filter separation. The known-pole fixture fit independently
measures how much of the envelope survives. High F0 sparsely samples resonances;
accurate recovery of F1 below F0 is intrinsically underdetermined.

## PhaseLockedSpectral: original peak-region candidate

Hann STFT frames overlap eightfold. Bin phase differences remove expected hop
advance to estimate instantaneous frequency. Local spectral peaks own regions
bounded by neighboring peak midpoints. Peak phase is propagated at the requested
frequency; nearby bins keep their relative phase, avoiding independent-bin
phase propagation. Source peak identity is matched within four bins between
frames. Spectral flux >0.55 triggers a phase reset. This is a conservative
transient heuristic, not a validated vocal attack classifier.

Peak regions translate in frequency while retaining lobe width. The window-center
phase ramp is removed before fractional-bin interpolation and reinstated at the
destination; otherwise adjacent Hann bins can cancel. Both channels use the same
peak map/phase rotation/gain, retaining their relative complex phase on the
tested stereo material. This is designed for one voice, not independent stereo
instruments or polyphony.

Three fixed iterations estimate an upper log-spectral envelope using cepstral
smoothing, with a tapered lifter below the source pitch period. The excitation
spectrum is implicitly whitened by dividing by the source envelope, then the
original envelope is restored at destination frequencies. Gain is bounded to
exp(+/-2.3), about +/-20 dB. This cap prevents extreme boosts but limits large
shifts. Source peaks beyond destination Nyquist are discarded; the final four
bins taper to zero. No artificial high-band excitation is generated on downward
shifts, so large downward shifts can lose spectral detail.

FFT storage and plans are prepared outside processing. Twenty-four frame tasks
are spread across one hop on the **sample** clock. This reduced the initial
single-callback FFT burst. No background worker is needed. Cold-start output
uses delayed dry until a complete OLA lattice exists; dividing a transformed
frame by a near-zero edge weight caused an attack amplification defect during
development. After warmup, the usual 3 ms fade enters correction. Internal
late-frame counters are asserted to be zero in regression tests.

## Unity, unvoiced material and transitions

At exact stationary unity or zero wet amount, all candidates return delayed dry
bit for bit. A 3 ms one-pole transition fades the two new engines into/out of
resynthesis; input-clock voicing prevents sustained unvoiced material from using
fictional periods. After release settles, the unvoiced samples are exact delayed
dry. Entering/leaving correction is not sample-exact during the fade and can
temporarily interfere with differently phased wet audio. The exported timeline
and event probes make that behavior measurable.

The M2 voicing decision can itself be late at a consonant boundary. Oracle
unvoiced tests isolate the transformation path; they do not prove that real
consonants will always be correctly classified. There is no new complex hybrid,
automatic engine selector, formant knob or retune parameter. Offline explicit
trajectories exercise 0–100 ms retune without changing M2's plugin semantics.

## Research basis and dependency boundaries

These are original implementations based on established principles, not copies
of any proprietary algorithm. [Moulines and Charpentier (1990)](https://courses.physics.illinois.edu/ece420/fa2017/PSOLA.pdf)
motivates pitch-synchronous waveform windows. [Laroche and Dolson (1999)](https://www.ee.columbia.edu/~dpwe/papers/LaroD99-pvoc.pdf)
motivates peak-region spectral transformations. [Röbel and Rodet (2005)](https://dafx.de/paper-archive/details/EDSTSaO17Gd_0LV03iwDfw)
and [Lenarczyk (2017)](https://www.isca-archive.org/interspeech_2017/lenarczyk17_interspeech.pdf)
motivate envelope estimation/restoration. The bounded implementations here are
not complete reproductions of those papers, and their published listening
results do not transfer to VocalPilot.

No external transformation engine ships or is linked. Optional external
comparisons were not run. Signalsmith Stretch's [MIT license](https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/main/LICENSE.txt)
requires retaining its notice if used; audit the pinned dependencies as well.
Rubber Band's [publisher licensing terms](https://breakfastquay.com/rubberband/license.html)
offer GPL v2-or-later or a separate commercial license; do not silently link it
into a proprietary distribution. JUCE's existing license obligations remain.

[ViSQOL](https://github.com/google/visqol) is left external. Follow its upstream
build instructions and run aligned dry/corrected files with its reference and
degraded-file options. Preserve the exact model/version and audio versus speech
mode in any report. Pitch correction intentionally changes the reference:
ViSQOL can penalize desired changes and is not an acceptance score by itself.
No ViSQOL or external-engine quality scores are reported here.
