# Milestone 2 architecture

The audio shifter is still the Milestone 1 dual-delay prototype. This milestone improves the decisions that drive it; it does not claim commercial resynthesis quality.

```
Left input -> SignalAnalysis -> PitchDetector -> TemporalPitchTracker
                                                     |
                                               TargetSelector
                                                     |
                                             CorrectionTrajectory
                                                     |
Stereo input --------------------> PitchShifter -> dry/shift transition -> output
```

TrackingPipeline owns the control stages and advances them at exactly the same sample times in the plugin, tests and WAV analyser. CorrectionEngine connects that pipeline to the unchanged PitchShifter. Parameters keep their existing IDs and meanings; old project state remains readable.

## Signal analysis: measure before deciding

SignalAnalysis removes DC with a 35 Hz high-pass and low-passes the analysis copy using two Butterworth biquads (fourth order, 2800 Hz). The audio itself is not filtered. Decimation uses the nearest integer factor to 12 kHz: 11025 Hz at 44.1 kHz and 12000 Hz at 48/96/192 kHz. The fixed filter is economical, not a brick-wall anti-alias filter; high-frequency leakage remains a limitation.

Input RMS is measured on the DC-rejected full-band copy over each approximately 10 ms hop. Filtered analysis RMS covers the newest 30 ms. These two measurements distinguish a sufficiently loud periodic signal from very quiet material and high-frequency-dominated noise. RMS is amplitude, not power or a voice probability.

## Why retain YIN-inspired detection?

The original detector already offered bounded cost and reasonable clean-tone accuracy. Plain autocorrelation can prefer multiples of the true period. NSDF/MPM is another reasonable normalized periodicity method; a spectral/harmonic system would add FFT sizing, peak selection and latency decisions before the temporal architecture was tested. This implementation retains YIN's cumulative mean normalized difference (CMNDF), adds normalized-correlation validation, and improves candidate selection. It is not a full implementation of every step in the original YIN paper, and no claim of beating MPM is made.

References: [de Cheveigné and Kawahara, YIN (2002)](https://pubmed.ncbi.nlm.nih.gov/12002874/) and [McLeod and Wyvill, A Smarter Way to Find Pitch (2005)](https://quod.lib.umich.edu/i/icmc/bbp2372.2005.107/1/--smarter-way-to-find-pitch?page=root%3Bsize%3D75%3Bview%3Dtext). These establish the methods; all reported VocalPilot performance comes from this repository's measurements.

PitchDetector keeps a preallocated ring of the newest comparison window plus the longest lag (approximately 46 ms altogether). Samples are arranged newest-first so the 30 ms comparison uses recent audio. For each lag tau:

- Squared difference: d(tau) = sum((x[i] - x[i+tau])^2).
- CMNDF: d'(tau) = d(tau) / mean(d(1)..d(tau)).
- Normalized correlation: n(tau) = 1 - d(tau) / sum(x[i]^2 + x[i+tau]^2).

The last expression equals twice cross-correlation divided by paired energy. A perfect repeated waveform has d'=0 and n=1. Prefix sums avoid recalculating paired energy inside every lag comparison.

Candidate generation finds local CMNDF minima. Parabolic interpolation estimates both fractional lag and valley depth. Selection chooses the shortest candidate within **0.05 normalized difference units** of the best interpolated valley. This allows small sampling/fit differences between equivalent periods while preferring a longer fundamental period if it explains materially more signal than a strong second harmonic. The tolerance is an engineering policy tested on the published fixtures, not a universal perceptual constant. If only even harmonics exist, the waveform actually repeats at twice the nominal missing fundamental; periodicity alone cannot uniquely infer the intended lower note.

## Confidence and voicing

Confidence is clamp(min(1 - interpolated CMNDF valley depth, interpolated normalized correlation), 0, 1). Correlation is evaluated at the same fractional lag with a quadratic fit. Interpolating quality avoids penalizing high pitches merely because their period falls between samples. Confidence measures periodic evidence, **not the probability that the octave is correct**. Hum, instruments and other periodic sounds can score highly.

A raw estimate is valid/voiced only when all gates pass:

| Gate | Threshold |
|---|---|
| Full-band RMS | At least 0.001 (approximately -60 dBFS) |
| Filtered RMS | At least 0.0005 (approximately -66 dBFS) |
| Filtered/full-band RMS ratio | At least 0.15 |
| Periodicity confidence | At least 0.80 |
| Nominal frequency | 65–1000 Hz, with 0.5% interpolation tolerance (64.675–1005 Hz) |

Invalid estimates use zero Hz/MIDI and valid=false. Confidence can be nonzero for rejected periodic candidates; silence below the gate produces zero confidence. Raw means accepted detector output before temporal filtering, not every internal candidate.

## Temporal tracking and octave mitigation

TemporalPitchTracker consumes only analysis events, never repeated copies of a frame. It requires confidence >=0.85 and two mutually consistent credible frames to acquire a pitch. For motion within three semitones it applies a short confidence-dependent one-pole update: alpha = 0.65 + 0.25 * confidence. At high confidence it follows 90% of the new estimate each hop, retaining vibrato and slides without a long median buffer.

A jump larger than three semitones needs three consistent frames; an approximately one-octave jump (12 +/- 0.75 semitones) needs four. Consecutive pending estimates must agree within 0.75 semitones. An isolated large outlier therefore does not move the tracked pitch, while a genuine octave can still be accepted after sustained evidence. Repeated, confidently wrong octave estimates can still win; this is mitigation, not a guarantee.

Tracking states:

| State | Meaning and correction policy |
|---|---|
| Unvoiced | No retained pitch, no target, no new correction |
| Acquiring | One credible frame so far; no correction |
| Tracking | Accepted trajectory; confidence controls reliability |
| Holding | Brief uncertainty or a pending large jump; pitch memory retained but reliability zero |

At most two missing/uncertain frames retain pitch memory; the third clears it. Reliability is clamp((confidence-0.80)/0.15, 0, 1) while Tracking and zero otherwise. Thus 0.95+ confidence supports full strength; weaker evidence reduces correction. Unvoiced material does not receive a new arbitrary pitch interval. The previous correction releases smoothly to zero while the wet transition also falls.

## Musical targeting and hysteresis

NoteTarget remains the stateless major/natural-minor nearest-note search (A4=440 Hz, MIDI 60=C4, exact ties choose the lower note). TargetSelector adds memory. Initial selection uses the tracked pitch immediately after tracker acquisition. Changing an established target requires a new candidate to be at least **0.40 semitones closer** than the current target: equivalently, 20 cents beyond their midpoint. That condition must hold for two consecutive credible analysis frames.

This produces different forward and reverse switching thresholds. Oscillation within the midpoint deadband retains the established note; a real crossing still switches. It is not a full vibrato model. Wide vibrato spanning both thresholds can switch notes. Changing key/scale invalidates the held target at the next analysis event (about 10 ms maximum parameter-response delay).

## Correction trajectory and audio latency

Requested cents = 100 * (target MIDI - tracked MIDI) * strength * reliability, bounded to +/-1200 cents. CorrectionTrajectory keeps the existing 25 ms one-pole cents smoother and 5 ms dry/shift transition. A one-pole time constant is not a fixed delay: it reaches 63% in 25 ms, 90% in about 58 ms and 98% in about 98 ms. The pitch ratio remains 2^(smoothed cents/1200). Near unity the single dry tap avoids stationary two-tap comb filtering.

Measured clean-fixture detector acquisition is about 50 ms; target acquisition about 60 ms. C4→D4, E4→G4 and A3→A4 target transitions take 30–39, 39–40 and 69–70 ms respectively. The cents smoother responds after that decision. These are **control response** measurements, distinct from audio delay. PitchShifter still reports a nominal 992 samples (20.67 ms) at 48 kHz; its instantaneous granular delay varies, so compensation is approximate during shifting. Bypass and zero strength retain the exact fixed-delay dry path after transition.

CorrectionEngine also bounds pathological finite inputs above +/-16 before delay arithmetic; ordinary audio is unchanged. NaN/Inf become zero. This defensive behavior prevents finite floating-point extremes from overflowing interpolated audio.

## Real-time and UI boundaries

All detector/tracker/target storage is fixed size. Only PitchShifter allocates, during prepare. Analysis loops have fixed prepared bounds; no logging, file/network I/O, locks, strings or GUI operations occur in the callback. The allocation test instruments C++ new/new[] including aligned forms in the pure core. It does not instrument every possible C allocator or the entire operating system; code inspection complements that test.

The processor publishes independent lock-free float atomics. The editor refreshes at 15 Hz and formats note names and state text on the message thread. A UI refresh can mix adjacent block readings; it is a diagnostic display, not synchronized telemetry. The offline tool instead exports sample-clock-aligned complete pipeline snapshots.

## Remaining weaknesses

Fixed gates are not adaptive to microphones or noise floors. Periodicity does not prove human voicing. Breathy/creaky phonation, strong accompaniment, room echoes, extremely weak odd harmonics, fast attacks and extreme transitions remain unvalidated on humans. Small erroneous jumps inside three semitones are smoothed rather than explicitly rejected. Synthetic success is only a foundation for the real-vocal workflow in [OFFLINE-ANALYSIS.md](OFFLINE-ANALYSIS.md). The unchanged granular shifter still colors and smears audio.
