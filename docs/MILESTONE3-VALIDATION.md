# Milestone 3 validation

Validation: 2026-09-08 local / 2026-09-09 UTC. Windows x64, MSVC 19.44,
CMake 3.31.8, pinned JUCE 8.0.12, Release; Intel Core i7-12700K.
Python 3 / NumPy 2.3.5 for offline
measurements. Branch: milestone-3-hq-resynthesis. Baseline: 3b29384.

## Executive result

**Two new transformation paths are implemented, compared and available in the
VST3. Milestone 3 sonic acceptance remains OPEN.** Both materially outperform
the reference on the measured stationary synthetic pitch and several timbre
metrics. Neither has been judged on a real-human vocal corpus or a controlled
listening panel. Do not describe this as professional/studio quality or a match
for commercial tuning products.

The pitch-synchronous candidate is the first recommendation for further vocal
listening because of lower CPU cost, strong large-shift envelope preservation,
and favorable dynamic proxies. The spectral candidate is retained: it has
better envelope-distance results on the tested 10–50 cent corrections. There
is no universally established winner. Legacy remains the saved/default choice
until the listening comparison is reviewed; new engines are selectable.

All **10 automated suites pass**. No original M1/M2 DSP/tracking assertion was
removed. The processor latency assertion intentionally changes from 992 to 4800
samples at 48 kHz, matching the HQ architecture. The binary-host test now loads
and processes all three engines. A fresh REAPER scan recognizes version 0.3.0;
actual REAPER track playback remains manual.

## Algorithms evaluated and why

| Engine | Reason for evaluation | Formant strategy |
|---|---|---|
| LegacyGranular | Retained M1 reference and historical regressions | None; waveform time scale changes within moving grains |
| PitchSynchronous | Exploit reliable M2 F0 and waveform epochs for one voice | Preserve waveform time scale inside two-source-period windows |
| PhaseLockedSpectral | Different failure modes and explicit harmonic-region coherence | Pitch-adaptive cepstral envelope extraction/restoration |

Implementations are original and based on published DSP principles. No
proprietary source was copied or reverse engineered. Optional Signalsmith,
Rubber Band and ViSQOL comparisons were **not run**, and no new external engine
ships. Licenses and research sources are linked in the architecture document.

## Architecture

See [MILESTONE3-ARCHITECTURE.md](MILESTONE3-ARCHITECTURE.md) for equations,
controls, epoch refinement, spectral phase/envelope rules and timing. M2 tracking
headers and the original PitchShifter.h are unchanged. Every offline engine
receives the same input-clock control trajectory. The legacy adapter adds
nominal control/delay alignment and the same short transition fade as the new
candidates; it does not add a new shifting algorithm or envelope preservation.
Historical M1/M2 tests still exercise the original complete path.

## Fixture and metric definitions

The main sweep contains **372 one-second synthetic fixtures / 1116 wet outputs**.
At 48 kHz: F0=90/130/220/440 Hz, three vowels, zero and +/-0.1/1/5/10/25/50/100/
200/300/500/700/1200 cents. At 44.1 and 96 kHz: F0=90/220/660 Hz, three vowels,
-700/-100/+25/+700 cents. Source harmonics are analytically filtered through
three known pole pairs, with excitation amplitude proportional to 1/sqrt(k).
The signal is band-limited below 0.47*rate and normalized to 0.16 peak. This is a
source/filter *synthetic model*, not a recording of any singer or gender.

| Vowel | F1 Hz | F2 Hz | F3 Hz | Pole bandwidths Hz |
|---|---|---|---|---|
| ah | 730 | 1090 | 2440 | 90 / 110 / 160 |
| ee | 270 | 2290 | 3010 | 90 / 110 / 160 |
| oo | 300 | 870 | 2240 | 90 / 110 / 160 |

Stationary scoring uses 250–850 ms after nominal delay removal. Envelope RMSE
compares measured target-harmonic amplitudes against the known analytic envelope,
removing an overall dB gain offset. It is not waveform correlation or a perceptual
metric. F1/F2/F3 fits use the known excitation slope and bandwidths, multiple
starting points and bounded pole-frequency search. Those priors make this a
fixture-specific measurement, not an independent real-voice formant tracker.
Search bounds are 0.45–1.8 times each nominal pole; large legacy shifts may exceed
them, understating that engine's fitted formant error. High F0 leaves resonances
undersampled, especially when F1 is below F0. Interpret the full spectrum and fit
residual alongside fitted poles. Every row includes signed Hz and percentage
errors in [milestone3-quality.csv](milestone3-quality.csv).

## Pitch accuracy

The spectral peak measurement searches near the requested fundamental, so its
sub-cent values alone would not establish correct F0. A **separate wide-search
periodicity audit** uses FFT-derived normalized differences over 30–2100 Hz,
without the requested target constraining the search. It rejects weak periodicity
and reports conditional error plus coverage. It can still make octave errors;
this is an estimator check, not electroglottographic truth. Audit: 3 rates x
3 F0s (90/220/660) x 3 vowels x 7 intervals (-1200,-700,-100,25,100,700,1200).


| Engine | Accepted outputs | Median abs cents | P95 | Max |

| --- | --- | --- | --- | --- |

| Legacy | 183/189 | 1.371 | 2311.440 | 3684.410 |

| Pitch synchronous | 189/189 | 0.010 | 0.206 | 0.479 |

| Spectral | 189/189 | 0.010 | 0.205 | 0.436 |


Legacy's large errors include periodicity/sideband ambiguities, not merely
small tuning offsets. Rejected outputs do not count as zero error. Raw results:
[milestone3-pitch-audit.csv](milestone3-pitch-audit.csv).

Requested-near spectral accuracy and envelope distance by correction range:


| Abs cents | Engine | Outputs | Target-near pitch P95 cents | Envelope RMSE median dB |

| --- | --- | --- | --- | --- |

| 0.1–5 | Legacy | 72 | 0.166 | 0.408 |

| 0.1–5 | Pitch synchronous | 72 | 0.004 | 0.567 |

| 0.1–5 | Spectral | 72 | 0.004 | 0.610 |

| 10–50 | Legacy | 90 | 7.142 | 2.803 |

| 10–50 | Pitch synchronous | 90 | 0.004 | 1.940 |

| 10–50 | Spectral | 90 | 0.004 | 1.066 |

| 100–300 | Legacy | 90 | 41.858 | 5.184 |

| 100–300 | Pitch synchronous | 90 | 0.004 | 2.706 |

| 100–300 | Spectral | 90 | 0.003 | 2.992 |

| 500–1200 | Legacy | 108 | 185.254 | 14.488 |

| 500–1200 | Pitch synchronous | 108 | 0.003 | 2.151 |

| 500–1200 | Spectral | 108 | 0.003 | 9.078 |


## Formant preservation

Aggregate moderate corrections (10–200 cents absolute). Values are medians of
absolute fitted errors; full rows retain the sign. This demonstrates improvement
within this fixture model, not preservation of a human singer's identity.


| Engine | F1 Hz | F1 % | F2 Hz | F2 % | F3 Hz | F3 % |

| --- | --- | --- | --- | --- | --- | --- |

| Legacy | 20.000 | 5.034 | 43.500 | 3.448 | 79.500 | 3.189 |

| Pitch synchronous | 7.000 | 1.781 | 12.000 | 0.826 | 13.000 | 0.533 |

| Spectral | 7.000 | 1.426 | 16.500 | 1.164 | 35.500 | 1.391 |


The spectral candidate has the lower envelope RMSE for 10–50 cent corrections;
pitch synchronous is better for larger intervals in this sweep. At 0.1–5 cents,
legacy's envelope metric is actually lower than either candidate. Therefore the
claim that every tiny correction is cleaner is **not established**. The two new
paths have more accurate requested periodicity and substantially lower level
modulation across many tested moderate shifts, but listening must weigh those
tradeoffs.

Epoch-refinement ablation: on the nine 130-Hz quick fixtures, predicted marks
alone yield envelope RMSE median 1.000 dB versus 1.294 dB with local peak refinement.
F1/F2/F3 fitted percentage errors improve with refinement (3.704/0.917/0.287% to
1.111/0.642/0.123%). Pitch differences are negligible. This mixed result does not
establish a universal benefit from peak refinement, especially on real voices.
See milestone3-no-refinement-summary.json. Keep the renderer ablation option.

## Spectral artifacts and harmonic quality

Inharmonic energy is power outside +/-8 Hz of the requested harmonic grid,
using a Hann-windowed stationary segment. It conflates modulation sidebands,
leakage and noise; it is not a psychoacoustic threshold. The grid metric can miss
an octave ambiguity, which is why the wide-search audit is also retained.

Moderate corrections (10–200 cents):


| Engine | Median off-grid power dB | Median RMS range dB | Max abs RMS gain dB |

| --- | --- | --- | --- |

| Legacy | -50.673 | 3.531 | 5.146 |

| Pitch synchronous | -52.877 | 0.591 | 1.150 |

| Spectral | -53.185 | 0.602 | 1.483 |


RMS modulation uses 20 ms windows and includes ordinary finite-window variation
of a periodic waveform. Larger shifts can change level markedly: do not confuse
gain-normalized envelope accuracy with level preservation. Worst absolute RMS
gain over the complete sweep is reported below; no automatic gain correction
was used to hide it.


| Engine | Max absolute RMS gain dB |

| --- | --- |

| Legacy | 5.146 |

| Pitch synchronous | 11.849 |

| Spectral | 6.833 |


## Variable-ratio tests and retune

The event suite includes zero crossings, source vibrato, slow/fast octave slides,
+25 -> +700 -> -700 cent jumps, voiced/noise transitions, breathy noise mixtures,
and abrupt gated attacks at 44.1/48/96 kHz. Trajectories are oracle controls to
isolate transformation. An independent 40 ms sinusoidal search measures dynamic
pitch with about 2.9-cent grid resolution. Values of zero mean a matching search
bin, not infinite precision. Unvoiced windows are excluded, not scored as pitches.
P95 below is the worst of the three per-rate P95 measurements.


| Event | Legacy P95 cents | Synchronous | Spectral |

| --- | --- | --- | --- |

| zero_crossing | 3.031 | 5.780 | 11.580 |

| vibrato | 2.883 | 0.000 | 5.780 |

| slide | 84.467 | 2.888 | 8.678 |

| fast_slide | 47.388 | 2.888 | 14.633 |

| hard_jump | 170.242 | 0.000 | 0.000 |

| breathy | 2.888 | 17.399 | 11.720 |

| attacks | 32.034 | 1.441 | 2.883 |

| consonants | 57.648 | 0.000 | 0.000 |


Offline retune tests cover 0,5,10,20,35,50,100 ms. Positive values are one-pole
time constants, not pure delays; zero is a deliberate step. All render finite
output without resetting an engine on ratio changes. The live plugin retains
M2's 25 ms smoother. Detection lag, smoothing response, FFT/window behavior and
host latency are different quantities.

## Transient quality and voiced/unvoiced behavior

111 event/engine configurations are recorded in
[milestone3-events.csv](milestone3-events.csv). On the synthetic consonant proxy,
all three engines preserve the tested plosive peak timing (0 ms offset). After
a 50 ms transition collar, their unvoiced RMSE against aligned dry rounds to zero.
The largest 0.350 adjacent-sample step in that fixture is the deliberately
inserted input plosive, not an output-only click. These results use **oracle
voicing**. They do not prove M2 will classify a human consonant in time.

Voiced-event worst adjacent-sample differences (excluding the inserted plosives):


| Engine | Max adjacent step |

| --- | --- |

| Legacy | 0.011 |

| Pitch synchronous | 0.009 |

| Spectral | 0.009 |


The new paths use time-aligned dry release, but differently phased wet/dry fades
can still color attacks. Spectral flux resets and peak searches are heuristics.
No listener has rated double attacks, consonant intelligibility, metallic sound
or graininess. Those acceptance items remain open.

## Zero-shift transparency and time-domain safety

Exactly zero correction is sample-exact delayed dry in all three engine tests
at 44.1/48/96 kHz, including random stereo input and anti-phase channels. PCM
renderer identity and explicit trajectory replay also pass. Continuous tiny
shifts +/-0.1,1,5 cents are measured, not forced through a unity deadband.
Transitions are short fades and are not claimed sample-exact while settling.

Twelve partitions (1,7,31,32,63,64,127,128,255,256,511,512) produce identical
sample results for fixed sample-clock controls. Dynamic ratio, anti-phase stereo,
engine selection and NaN/Inf sanitization tests pass. FFT inverse reconstruction
error is <1e-12 in its test. Prepared spectral scheduling has zero late frames
at all three regression rates. The cold-start lattice now stays dry until fully
covered, preventing near-zero window division from amplifying attacks.

## Aliasing

Two high-frequency single-partial probes near 0.34 and 0.43 times sample rate
are shifted upward an octave. We measure power around the predicted fold image
relative to the input sinusoid, not relative to an output that may be silent.
Worst (least negative) result across both probes and all three rates:


| Engine | Fold image relative to input dB |

| --- | --- |

| Legacy | -0.000 |

| Pitch synchronous | -139.781 |

| Spectral | -94.588 |


The spectral path discards out-of-range regions rather than folding them. The
synchronous path repeats unresampled waveform windows, which is not equivalent
to translating each original high harmonic upward. Low fold energy does not
prove ideal anti-aliasing for every waveform; OLA modulation and interpolation
still deserve a broader high-frequency audit. No oversampling claim is made.

## Real-vocal test results

**No human-vocal fixtures tested.** The permission-cleared corpus directory,
category placeholders and manifest template exist; recordings are intentionally
not invented or downloaded. Real-vocal timbre, breathiness, articulation and
tracking/resynthesis interactions remain unvalidated. Use your own recording.

## Blind listening results

**Zero listeners, zero scored trials.** The implemented workflow generates A/B/C/D
plus a dry reference, randomizes identities, RMS-matches with shared peak headroom,
and withholds its reveal command until all nine ratings are entered. It is a
blinded rating harness, not a completed ABX experiment. Scores cannot be inferred
from the measurements above. See [MILESTONE3-LISTENING.md](MILESTONE3-LISTENING.md).

## Latency

| Component | Measured/defined behavior |
|---|---|
| M2 raw acquisition | Historical clean-fixture 49.89–50 ms; unchanged |
| M2 first target | Historical clean-fixture 59.86–60 ms; unchanged |
| Retune | 25 ms time constant, about 58 ms to 90%; unchanged |
| Legacy native nominal audio delay | 32 + floor(0.040*rate)/2; moving grains vary |
| HQ host delay at 44.1 kHz | 4608 samples / 104.4898 ms |
| HQ host delay at 48 kHz | 4800 samples / 100 ms |
| HQ host delay at 96 kHz | 9600 samples / 100 ms |
| Spectral cold startup | Dry until one complete N-sample source lattice, then 3 ms fade |
| Engine selector | 20 ms one-pole fade; no latency change |

Host delay compensation is fixed across all engine selections. The additional
hop at 44.1 kHz is necessary for scheduled frame work; rounding it down to 100 ms
would make early frame output late. Pitch control lag is not added a second time
to host latency. The future low-latency mode is an architectural extension, not
implemented by reducing these windows in this milestone.

## CPU

Two seconds per engine/configuration, stereo, all startup and complete callbacks
included. 44.1/48/96 kHz x 32/64/128/256/512 samples. Timing excludes fixture
construction and CSV output. Full data: [milestone3-cpu.csv](milestone3-cpu.csv).
Engine IDs: 0 legacy, 1 synchronous, 2 spectral, 3 warm bank, 4 M2 tracking plus
warm bank (the plugin core, excluding host/GUI overhead). The following table
shows the complete core; mean/P95/P99/max are milliseconds.


| Rate | Block | Mean | P95 | P99 | Max | Max budget % |

| --- | --- | --- | --- | --- | --- | --- |

| 44100 | 32 | 0.0502514 | 0.1117 | 0.1349 | 0.1983 | 27.3282 |

| 44100 | 64 | 0.10206 | 0.1571 | 0.2095 | 0.4562 | 31.435 |

| 44100 | 128 | 0.203472 | 0.2832 | 0.3437 | 0.6312 | 21.7468 |

| 44100 | 256 | 0.401008 | 0.4829 | 0.5334 | 0.6196 | 10.6736 |

| 44100 | 512 | 0.816107 | 0.9158 | 0.9932 | 1.0501 | 9.04481 |

| 48000 | 32 | 0.0510449 | 0.1132 | 0.1375 | 0.2415 | 36.225 |

| 48000 | 64 | 0.102352 | 0.158 | 0.1818 | 0.2574 | 19.305 |

| 48000 | 128 | 0.202329 | 0.2921 | 0.3531 | 0.4918 | 18.4425 |

| 48000 | 256 | 0.407924 | 0.5144 | 0.6476 | 0.8211 | 15.3956 |

| 48000 | 512 | 0.97397 | 1.5102 | 2.1488 | 2.8996 | 27.1837 |

| 96000 | 32 | 0.0602401 | 0.1679 | 0.2081 | 0.9554 | 286.62 |

| 96000 | 64 | 0.108878 | 0.2542 | 0.2983 | 0.6634 | 99.51 |

| 96000 | 128 | 0.217792 | 0.3227 | 0.3943 | 0.6963 | 52.2225 |

| 96000 | 256 | 0.434474 | 0.5952 | 0.6804 | 1.2431 | 46.6163 |

| 96000 | 512 | 0.870541 | 1.0804 | 1.2255 | 1.6123 | 30.2306 |


Worst observed budget fraction across the 15 configurations for each path:


| Path | Max observed budget % |

| --- | --- |

| Legacy | 50.535 |

| Synchronous | 48.465 |

| Spectral | 139.110 |

| Warm bank | 161.190 |

| Complete core | 286.620 |


**Small-buffer deadline acceptance is not met.** Observed maxima exceed 100%
of the callback budget in some configurations even though average cost is
practical. These runs include scheduler/preemption noise and are not hard
real-time guarantees. The initial unscheduled spectral implementation peaked at
835% of a 32-sample budget at 96 kHz. Distributing frame work across samples
substantially reduced bursts, but did not eliminate every observed deadline
miss. Start manual playback at 256 samples; do not claim 32-sample robustness.
The warm bank intentionally pays for all engines for switching comparisons.
A shipping mode should run only its selected path, after a tested transition
policy is designed. That optimization has not been substituted for quality work.

## Real-time safety

The allocation test now exercises 3000 historical-core and 3000 HQ-core variable
callbacks across three rates, with engine changes. Zero operator new/new[] calls,
including aligned forms, occur while processing is watched. It does not hook all
C allocation APIs or the operating system. Code inspection confirms no audio
thread file I/O, logging, GUI access, networking, locking or unbounded loops.
Buffers/plans allocate in preparation. Core safety and observed deadline behavior
are separate: allocation-free does not imply deadline-safe.

## Host test results

All three actual VST3 paths load, create the editor and move a 432 Hz input toward
A4 within the retained 20-cent output test tolerance and 0.3 peak bound. The test
releases resources between independent runs; repeating prepare alone did not
reset the hosted processing state and contaminated the first multi-engine test
with the prior tone's discontinuity. Parameter state roundtrip includes the new
engine ID; the original four IDs and VST3 class ID are unchanged.

REAPER 7.57 fresh isolated scan recognizes VocalPilot, class
ABCDEF019182FAEB56706A7456703031. The scanned bundle is version 0.3.0. No normal
REAPER configuration was modified. **No real-device playback, recording or REAPER
track-level listening pass is claimed.** The manual procedure remains required.
The editor screenshot was rendered and visually checked.

## Regressions and development findings

Ten suites pass: original DSP, tracking, expanded allocation, transformation
invariants, processor/state/editor, original WAV analysis, numerical quality
regression (NumPy), comparison renderer, original WAV/reference validation and
three-engine VST3 host. Baseline seven-suite evidence is retained separately.

Resolved findings: octave-down normalization erased the synchronous fundamental;
two-source-period windows and non-normalized OLA fix it. Initial spectral frame
bursts exceeded short callback budgets; scheduled work reduces those bursts.
44.1 kHz needed N+H delay rather than a fixed 100 ms. Cold spectral lattice edges
could amplify an attack; delayed dry now covers startup. The legacy comparison
adapter uses a short fade too, so oracle unity crossings do not create an
artificially abrupt reference transition. No failing quality test was disabled
to claim acceptance. Known quality/CPU shortcomings remain reported.

## Engine bake-off and recommendation

| Criterion | Legacy | Pitch synchronous | Spectral |
|---|---|---|---|
| Stationary F0 | Some large-shift/sideband failures | Wide audit <0.5 cent max | Wide audit <0.5 cent max |
| Formants/envelope | Moves envelope with shifting | Strong moderate/large synthetic preservation | Strong small-shift result; large downshifts lose detail |
| Vowels/naturalness | Listening pending | Listening pending | Listening pending |
| Transients/consonants | Oracle dry proxies preserved | Oracle dry proxies preserved | Oracle dry proxies preserved; cold-start guard |
| Continuous ratios | Some larger pitch errors | Favorable slide/vibrato proxies | More dynamic lag in tested slides |
| Hard tuning | Artifact-prone reference | Finite coherent test transitions | Finite coherent test transitions |
| Tiny shifts | Lowest envelope distance in 0.1–5 cent subset | Better periodicity, subjective result unknown | Better periodicity, subjective result unknown |
| Aliasing probes | Severe fold in one probe | Low measured fold | Out-of-band regions removed |
| Latency in bank | Common HQ delay, variable grain positions | Common HQ delay | Common HQ delay and cold warmup |
| CPU | Cheapest | Much cheaper than spectral | Average practical, short-block spikes remain |
| Allocation safety | Pass | Pass | Pass |
| Blind listening | Not performed | Not performed | Not performed |

**Recommended production engine: none approved yet.** Pitch synchronous is the
provisional development lead, not a declared perceptual winner. Retain the
spectral candidate for the small-shift listening comparison. No automatic hybrid
is justified by the present evidence. Low-latency shipping architecture and
single-engine CPU operation should follow a real vocal comparison, not precede it.

## Known limitations and acceptance status

Real-vocal performance, consonant intelligibility and small-shift audibility are
open. High F0 envelope identification is underdetermined. Large intervals can
change level strongly, despite correct F0. Epoch marks are waveform peaks, not
validated glottal closures. Spectral envelopes have bounded gain and cannot
restore absent harmonics perfectly. Phase/time alignment does not guarantee
inaudible crossfades. CPU maxima at small blocks fail a strict deadline criterion.
Other desktop platforms, broad plugin validators, external quality references
and actual listening comparisons are untested.

The implementation, synthetic evidence, reproducible renderer, blind workflow,
VST3 integration and documentation are delivered. **The full Milestone 3
acceptance checklist is deliberately not marked complete.** Your dry-vocal
listening and remaining deadline/quality work are required before promotion.

## Recommended Milestone 4

Do not start it automatically. First close Milestone 3 with permission-cleared
vocal recordings and blinded ratings, resolve reproducible audio artifacts and
validate REAPER playback at the intended buffer sizes. Only then choose a
shipping engine, optimize single-path CPU, improve epoch/envelope estimation
where that corpus demonstrates a need, and design a separately measured
low-latency mode. No feature expansion is warranted by this report.

## Reproduce

Build per README with Python/NumPy and VOCALPILOT_HOST_TEST=ON, then run CTest.
Run transformation_quality.py for the 372-fixture sweep, transformation_events.py
for 111 event outputs, transformation_pitch_audit.py for the 189-case wide-search
audit, and transformation_benchmark for 75 timing configurations. Use --quick
--no-refine for the epoch ablation. Reports contain measurements, not proprietary
algorithm claims or invented listening scores.
