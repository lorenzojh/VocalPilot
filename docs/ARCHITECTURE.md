# Architecture

```
Left input -> low-pass/decimate -> YIN estimate -> MIDI -> scale target
                                                            |
                                              interval * strength
                                                            |
                                                25 ms smoothing
                                                            |
Stereo input -> linked fractional-delay pitch shifter -> dry/wet transition -> output
                      |                                  ^
                      +-> fixed latency dry tap ---------+
```

## PitchDetector.h

Two one-pole low-pass sections reduce high-frequency content before integer decimation to approximately 11–12 kHz at standard rates. This is a deliberately inexpensive analysis filter, not a brick-wall anti-alias filter. A 1024-sample ring is analysed every 10 ms. The difference function compares 512 samples at each lag and normalizes by the cumulative mean (YIN). The first valley under 0.15 is selected and refined by parabolic interpolation. The accepted lag interval covers 65–1000 Hz. A roughly −50 dBFS filtered RMS gate rejects silence. No qualifying valley means unvoiced, so stale estimates are cleared at the next analysis.

Return value: frequency and confidence, with zero frequency for unvoiced. Replace this component with an FFT-accelerated detector or better voicing estimator later without changing targeting or shifting. The bounded nested analysis loop currently runs on the audio thread; measure worst-case callbacks, not just average CPU, before shrinking the host buffer.

## NoteTarget.h

Frequency-to-MIDI uses `69 + 12 log2(hz/440)`. Chromatic deviation is `100 * (midi - round(midi))`. Major intervals are `{0,2,4,5,7,9,11}`; natural minor intervals are `{0,2,3,5,7,8,10}`. Search nearby integer MIDI pitches across octaves and select the permitted pitch with minimum absolute distance. The selected key is an offset in semitones from C. Sharps are used consistently for display.

The requested correction is `100 * (targetMidi - detectedMidi) * strength`. At 50%, the output moves halfway in cents toward the target. A future target tracker can add hysteresis and note-transition policy here.

## PitchShifter.h

Pitch ratio is `2^(smoothedCents/1200)`. A circular buffer is read with two linearly interpolated taps, separated by half a 40 ms grain span. Delay changes by `1-ratio` samples per sample, so the read rate is the requested ratio. Complementary raised-cosine weights hide each tap's wrap. Identical phase and weights across both channels preserve stereo relationships. This is a basic granular/delay shifter, not PSOLA or a phase vocoder.

The delay spans 32 samples through 32 + 40 ms. The fixed dry tap is at the midpoint. Two taps at ratio 1 would cause comb filtering; the engine transitions to a single dry tap near unity. A better shifter with fixed effective latency and formant-aware processing is the most valuable future upgrade.

## CorrectionEngine.h

Coordinates the pure-C++ components sample by sample. A 25 ms one-pole smoother controls cents, and a 5 ms smoother controls the transition between dry and shifted output. Bypass, unvoiced input and zero strength select dry. Internal shifter/detector state continues advancing during bypass. Non-finite input samples are replaced by zero to keep the delay/filter state usable. Dry bypass is bit-identical to the input delayed by the reported latency once the transition settles. At bypass startup, the delay line naturally emits silence until filled.

## PluginProcessor / PluginEditor

The processor owns DSP and JUCE's AudioProcessorValueTreeState. Stable versioned parameter IDs are `key`, `scale`, `strength`, `bypass`. XML state is serialized using JUCE's binary wrapper and restored only for the expected root tag. Mono and stereo matching input/output layouts are accepted; instruments, MIDI and double-precision processing are not advertised.

The editor uses standard JUCE parameter attachments. Formatting and note names happen only on the UI thread. Meter fields are independent lock-free atomic snapshots: a display refresh can combine adjacent block readings, which is acceptable for diagnostics but not suitable for sample-accurate data capture.

## Suggested milestone 2

Profile worst callback times with actual vocals and small buffers, improve voiced/unvoiced tracking and target hysteresis, then replace the shifter. Preserve the DSP interfaces and add recorded-vocal regression fixtures. Do not infer production readiness from clean sine-wave tests.
