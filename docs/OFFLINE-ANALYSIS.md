# Repeatable WAV analysis

Build `vocalpilot_analyse` with the normal JUCE build. With a multi-configuration generator it is under `build/vocalpilot_analyse_artefacts/Release/`. The Windows distribution also includes it in `bin/`.

```powershell
.\bin\vocalpilot_analyse.exe "dry-vocal.wav" "analysis.csv" 0 major 100
python tools/compare_reference.py analysis.csv reference.csv
```

Arguments are input WAV, output CSV, optional key (0=C through 11=B), major/minor, and strength 0–100. The tool accepts WAV formats supported by JUCE, sample rates 8–192 kHz, and uses the left channel without resampling. Standard 44.1/48/96 kHz rates have the expanded benchmark coverage. A file shorter than acquisition time can produce a header with no data rows. The output CSV is overwritten, but passing the same input/output file is rejected. Audio is never changed.

The tool runs **TrackingPipeline**, exactly as the plugin does sample by sample, including confidence gating, target state and smoothing. Only the plugin subsequently calls PitchShifter. File reading and CSV formatting occur in this standalone process, never the VST callback. Offline buffers are 2048 samples; this does not change the analysis hop or pipeline results.

## Columns

| Column | Meaning |
|---|---|
| timeSeconds | End of the analysis frame on the input sample clock; not latency compensated |
| inputRms | Linear RMS of DC-rejected, full-band analysis input over the latest hop |
| rawFrequency / rawMidi | Accepted detector estimate before temporal tracking; zero when invalid |
| confidence | Periodicity quality, 0–1; not a calibrated probability of the correct octave |
| voiced / valid | Raw periodicity, level, spectral-balance and range gates passed |
| trackedFrequency / trackedMidi | Temporal estimate, possibly retained briefly during uncertainty |
| trackedValid | Whether pitch memory exists; inspect trackingState and reliability before using it |
| targetMidi / targetNote | Held musical target; −1 / empty when none |
| rawCorrectionCents | Unweighted target minus raw detector pitch, in cents |
| requestedCorrectionCents | Target minus tracked pitch, scaled by strength and reliability; zero when disabled |
| smoothedCorrectionCents | Current 25 ms correction trajectory, including its release toward zero |
| reliability | Correction support derived from confidence, or zero during acquiring/holding |
| trackingState | unvoiced, acquiring, tracking or holding |
| shiftMix | 0–1 transition between the fixed dry tap and granular output |

Zero-frequency sentinel values are not MIDI C−1 measurements. Do not include invalid/unvoiced or held pitches in accuracy summaries without an explicit policy. The tool emits frames only after initial acquisition buffering; preserve this missing startup interval when comparing with labels.

## Adding real vocals

Use dry, single-voice recordings you own or have permission to use. Start with several seconds each of a sustained vowel, low/high notes, breath, consonants, vibrato, slides and discrete notes. Avoid tuning, reverb and accompaniment. Save recordings in an ignored local directory such as `test-output/vocals/`. Do not commit identifiable/private recordings by default. For any redistributed dataset, include its source, consent/licensing terms, sample rate, channel choice and annotation method.

Run the same command, key, scale and strength for each revision. Keep CSV results outside Git in `test-output/`; commit only small, curated reports or permission-cleared fixtures. `tests/HarnessTests.py` checks real WAV decoding and output semantics at three rates, stereo selection, all three strength settings, reference comparison, and invalid arguments.

## Reference labels

A reference CSV has strictly increasing timestamps and these fields:

```csv
timeSeconds,frequency,voiced
0.150,220.0,1
0.160,220.0,1
0.170,0,0
```

Prefer independently labeled F0 (for example manually reviewed trajectories or electroglottography), not the plugin's own output as ground truth. Frequency must be positive when voiced. Omit unreliable label intervals instead of guessing. Document ambiguous octave labels and annotation uncertainty.

`compare_reference.py` uses the nearest reference timestamp within 6 ms by default (`--tolerance` changes this). It reports skipped and matched frames, voiced precision/recall, missing tracked pitches, median/P95/max absolute cents error, octave-sized errors and all gross errors above 600 cents. Pitch errors include only reference-voiced frames whose tracker state is `tracking`; held estimates are excluded and counted as missing. There is no automatic time shifting. Thus dynamic-pitch errors include causal control lag. Any separate latency-compensated experiment must report the chosen offset and also retain the causal result. No matched/pitched data yields null statistics, never invented zero error.

This utility is a preparation for real-vocal evaluation. The current report uses deterministic synthetic signals and the existing synthetic WAV, not human recordings.
