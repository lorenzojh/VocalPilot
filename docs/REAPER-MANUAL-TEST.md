# Milestone 2 manual acceptance with your own dry vocal

## Setup

1. Stop playback and remove older VocalPilot instances. Extract the Milestone 2 distribution; add its `bin` directory to REAPER's VST paths and re-scan. Avoid scanning two copies of VocalPilot. Load **VST3: VocalPilot (VocalPilot)** and confirm the UI says **Milestone 2**.
2. Use a dry monophonic recording on a track. A mono take on REAPER's stereo track is fine. A stereo file must carry the intended voice on the left channel. Disable other pitch FX and reverb for the test.
3. Set a known key/scale, initially C major. Start at **0% strength**, bypass off. Listen at a comfortable level. The detector and target diagnostics still run at 0% and in bypass.
4. Use a second tuner as a sanity check, but do not treat either detector as unquestionable ground truth. For direct A4 comparisons choose an A-centered phrase. Work at 48 kHz / 256 samples first, then test 128, 64 and 32 if your device supports them. Watch REAPER's performance meter and note dropouts.

## What to watch

- **Raw Hz / MIDI / note:** detector before temporal filtering. MIDI 69 is A4, 440 Hz.
- **Tracked Hz / MIDI:** pitch driving targeting. A brief erroneous raw octave should normally produce Holding, not an immediate tracked octave.
- **Confidence / Voiced:** periodic evidence. Clean vowels often approach 1; this is not proof that the chosen octave is correct.
- **State / Reliability:** Acquiring needs credible frames. Holding retains short-term memory but reliability becomes zero and correction releases. Tracking supports correction. Unvoiced has no trusted pitch.
- **Target:** sticky scale note. It should not chatter at a boundary.
- **Raw deviation:** cents from the nearest chromatic note, not the target.
- **Requested:** target minus tracked pitch, multiplied by strength and reliability. Positive raises pitch, negative lowers it.
- **Smoothed / Shift mix:** actual cents trajectory and transition to the shifted output. During unvoiced release, smoothed cents can briefly remain nonzero as shift mix falls.

## Listening checklist

| Recording segment | Action and expected observation |
|---|---|
| Sustained “ah”, “ee”, “oo” | Hold each for 1–2 seconds. After roughly 60 ms on clean synthetic signals, expect Tracking and a stable target. Test your voice rather than assuming that timing transfers exactly. |
| Low notes | Sustain comfortably near your lower range (nominal detector floor 65 Hz). Watch raw/tracked octave agreement and dropouts; do not strain to reach a prescribed pitch. |
| High notes | Sustain comfortably near the upper range. Watch for half-frequency locks or low confidence, especially with breathy tone. Nominal ceiling is 1000 Hz. |
| Slightly flat A | Sing below A4 with A permitted. Requested correction should be positive at 100%, about half as large at 50%, and zero at 0%. |
| Slightly sharp A | Sing above A4. Expect negative correction with the same strength relationship. |
| Vibrato | Use a natural A-centered oscillation. Tracked pitch should move while target remains A. A fixed commercial-sounding vibrato treatment is not expected. |
| Slides | Slide C4→G4 and A3→A4 if comfortable. Tracked pitch should follow continuously; targets should advance through allowed scale notes without octave spikes. |
| Rapid notes | Alternate two notes and make a genuine octave change. Record any delayed or missed changes. Synthetic C→D, E→G and A3→A4 transitions measured about 30–70 ms. |
| Breath | Isolate an exhale and a breath between vowels. Expect low confidence/unvoiced, reliability zero and shift mix releasing to zero. |
| Consonants | Alternate vowels with S/SH/T/P. Brief Holding is acceptable; a stable arbitrary target with substantial correction throughout sustained sibilance is a failure. |
| Boundary | In C major, slowly approach C# from C, hover around it, then move toward D. Repeat from D downward. The held target needs about 20 cents beyond the midpoint for two analysis frames before switching. Wide oscillation beyond both thresholds can legitimately switch. |
| Key/scale | Around C#, change C major to D major: C# becomes permitted. Around E-flat, change C major to C minor: E-flat becomes permitted. The old target is invalidated on the next analysis frame. |
| Bypass and strengths | Repeat the same phrase at 0%, 50%, 100%, then bypass. At 0%/bypass the delayed dry path should be unchanged after transition. Higher strength should move the sustained vowel farther toward the target. Low confidence deliberately reduces the requested amount. |

Repeat bypass toggles both during vowels and silence, save/reopen the project and confirm all four settings persist. Test both mono material and anti-phase stereo if relevant. Run several minutes of playback before trying input monitoring; do not record a “real-vocal pass” solely from the synthetic smoke script.

## Identify which stage failed

| Symptom | Likely area to investigate |
|---|---|
| Raw pitch is wrong by an octave, erratic on a stable vowel, or Voiced stays on during breath | Detector/voicing; save the WAV and inspect CSV confidence and raw pitch |
| Raw is reasonable but tracked pitch jumps or remains on an old note too long | Temporal tracker; inspect Holding duration and jump evidence |
| Raw/tracked agree with the voice but target chatters or is musically wrong | Key/scale selection or target hysteresis; inspect target transitions independently of audio |
| Target is correct but requested/smoothed correction overshoots, jumps abruptly or takes too long | Correction trajectory; requested should step, smoothed should approach without overshoot |
| All trajectories look right but audio beats, sounds metallic, smears consonants or changes vowel color | Existing granular shifter; these artifacts are expected limits of this engine and inform the next resynthesis milestone |
| Clicks/dropouts coincide with CPU overloads, even when pitch is stable | Real-time performance, device buffer or host scheduling; record rate/block size and performance measurements |

Keep a log with timestamps, key/scale, strength, sample rate/block size and the suspect stage. Export the same WAV through the offline analyser for reproducible diagnosis. No Milestone 3 implementation is included here.
