# Milestone 3: own-vocal REAPER and blind comparisons

Human listening is still required. No listener scores or real-vocal recordings
have been supplied. The synthetic measurements are not a substitute.

## Build and identify

Build Release using the README instructions. Copy the whole VST3 bundle and
rescan it in REAPER. Avoid two scanned copies with the same plugin ID. The UI
must say **Milestone 3 | Transformation research** and offer Legacy reference,
Pitch synchronous and Spectral phase locked. Version is 0.3.0. Start with playback
at 48 kHz/256 samples. The plugin reports 4800 samples (100 ms) at that rate;
44.1 kHz reports 4608 (104.49 ms). This HQ prototype is not a low-latency monitor.

Load the same dry monophonic take on three duplicate tracks, select a different
engine on each, and keep key/scale/strength identical. Solo exactly one at a time;
summing corrected and dry signals can create comb filtering. Disable reverb,
compression, other tuning and automatic gain. Start below clipping, around
-18 dBFS RMS with comfortable peaks. Observe CPU/dropouts at 256, then 128,64,32.
Do not equate a passing test host with real audio-device deadline guarantees.

Confirm 0% strength and bypass return the same sound as dry once the transition
settles. Compare engine switches during silence first, then a vowel. A switch
does not change host compensation. Listen for coloration during its 20 ms fade;
the three engines need not have identical waveform phase.

## Record a 30–60 second dry passage

| Passage | Procedure | Listen/watch for |
|---|---|---|
| Sustained ah | Hold 2–3 seconds at a comfortable middle note | Vowel identity, grain beating, level fluctuation |
| Sustained ee | Repeat at the same note | Thin/metallic color and unstable narrow resonances |
| Sustained oo | Repeat at the same note | Lost low resonance, hollow tone |
| Low notes | Descend comfortably; do not strain | Octave errors versus doubled waveform |
| High notes | Ascend within your range | Altered identity and weak fundamental; sparse harmonics limit formant estimates |
| Small flat notes | Sustain 10/25/50 cents below a permitted target if practical | Positive correction, transparency at small shifts |
| Small sharp notes | Repeat above the target | Negative correction; compare level and color |
| Wide vibrato | Hold one target with vibrato | Warble/metallic modulation, target chatter versus shifter artifact |
| Slow slides | Slide smoothly over a fifth | Continuous pitch, no periodic flutter |
| Fast slides | Repeat quickly | Lag, phase breakup, attack smearing |
| Octave jump | Jump and hold each end | Tracker Holding versus transformation discontinuity |
| S/SH | Alternate vowels and isolated sibilants | Noise modulation, blurred consonants, dry-return timing |
| P/T/K | Speak then sing pa/ta/ka | Missing or doubled attacks, flamming, clicks |
| Breathy phrase | Sing a comfortable airy phrase | Unvoiced dropouts, unstable epochs, changed breath texture |
| Rapid melody | Sing several discrete notes | Pitch effectiveness and natural articulation independently |

Repeat 25%,50%,100% strength. Strength is cents amount, not retune speed. For
natural tests use small errors and M2's 25 ms smoothing. The offline trajectory
workflow below can drive 0/5/10/20/35/50/100 ms responses; 0 ms deliberately means
a step, not a DSP reset. Hard snapping should come from that trajectory, not
clicks or broken consonants. There is no new live retune control in this milestone.

## Exact shared-trajectory renders

From the project root, after building:

```powershell
.\build\vocalpilot_render_artefacts\Release\vocalpilot_render.exe my-dry-vocal.wav test-output\take-01 --key 0 --strength 100
python tools\blind_listening.py prepare test-output\take-01 test-output\listen-01
```

For Ninja/single-config builds the same JUCE artefact layout is used; locate the
executable under the configured build directory if your generator differs.
Use a new output folder each time. Add `--minor` for natural minor. No audio is
uploaded. The input must be 8–192 kHz and no longer than 600 seconds. The renderer
uses the left channel for tracking, preserves up to two channels, removes each
engine's common latency, and writes equal-length 24-bit PCM WAVs plus render.json
and controls.csv. Input gain must leave headroom; clipping aborts rather than
silently normalizing one engine. It renders the full input and flushes its delay;
there is no extra output tail beyond the input duration.

`--f0 220 --cents 25` bypasses tracking with a fixed *oracle* transformation
request, appropriate for known synthetic fixtures. It is not a valid way to
evaluate an arbitrary changing vocal. `--trajectory path.csv` replays one control
row per sample, with the exact header:

```csv
sample,sourceHz,ratio,voiced,confidence,amount
0,220,1.014545334938,1,1,1
```

Samples must be consecutive from zero with exactly the input length. The file
records input-clock control lag; it does not invent ground-truth vocal F0.
Every engine gets identical control records. Legacy's moving taps retain
variable instantaneous delay even though nominal delay is removed.

## Blind procedure

Listen only inside `listen-01`: A.wav through D.wav and reference-dry.wav.
The helper randomizes engine identities (including a hidden dry condition),
matches whole-file RMS, and applies a shared peak headroom reduction if needed.
This is approximate loudness matching, not LUFS or a perceptual loudness model.
Do not inspect the sibling reveal JSON before scoring. Use another person to
prepare packs for a stronger blind protocol. No engine metadata is put in WAVs.

Enter a pseudonymous listener ID and 1–5 scores in scores.csv: naturalness,
timbre, absence of metallic character/warble/graininess, consonants, transients,
transitions, and correction effectiveness. **5 is always better.** Enter N/A for
events absent from that take. Repeat with multiple passages and randomized
orders. Dry may win naturalness but fail pitch correction; score both separately.

```powershell
python tools\blind_listening.py reveal test-output\listen-01 test-output\listen-01-reveal.json
```

The command refuses to reveal until all rows are filled. This protects workflow,
not secrecy against someone opening JSON. It is a blinded rating harness, not a
statistically powered ABX trial. Record listener count, repeats, fixture hashes,
orders, playback chain and raw scores before claiming perceptual improvement.

## Diagnose the failure stage

| Symptom | Likely investigation |
|---|---|
| Chipmunk/deeper identity; correct F0 but strange vowels | Transformation/envelope preservation |
| Metallic or underwater | Spectral phase locking, moving peak regions, mixed phases |
| Periodic flutter | Epoch synchronization or varying ratio |
| Buzzing/doubled waveform | Epoch selection/overlap-add or octave tracking |
| Blurred consonants | Voicing latency, fade or transient handling |
| Click on note change | Phase/state continuity; compare raw and smoothed controls |
| Natural timbre but wrong pitch | Check raw/tracked/target trajectory first, then output F0 |
| Dropouts even on sustained tone | Callback cost/audio driver; capture rate/block and CPU meter |

Save/reopen the REAPER project to test all five parameters, test host bypass as
well as the plugin button, and play several minutes before judging stability.
Record timestamps of failures and keep original dry WAV plus controls.csv. Only
publish recordings for which redistribution is explicitly permitted.
