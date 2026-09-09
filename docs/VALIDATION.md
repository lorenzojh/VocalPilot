# Milestone 2 validation report

Validation date: 2026-09-08. **Synthetic evidence only; no human-vocal accuracy claim.**

## Build environment and baseline

Windows x64, MSVC 19.44 / toolset 14.44.35207, Windows SDK 10.0.26100.0, CMake 3.31.8, Ninja 1.12.1, pinned JUCE 8.0.12, Release. VocalPilot version is now 0.2.0; manufacturer/plugin IDs, four parameter IDs and state schema are unchanged.

Before implementation, branch main at cb87454 was clean and all three Milestone 1 suites passed (0.85 seconds total). Work proceeded on milestone-2-vocal-tracking. The baseline was recorded before any DSP edits. Historical Milestone 1 evidence remains in [MILESTONE1-VALIDATION.md](MILESTONE1-VALIDATION.md) and test-results.txt.

## Test results

All **7 CTest suites pass**: unchanged Milestone 1 DSP tests; expanded tracking tests; core allocation test; processor/state/editor tests (with added diagnostics assertions); WAV analysis smoke test; end-to-end WAV/CSV/reference validation; and the actual VST3 binary host test. No existing assertions were removed. The optional Python test requires a discovered Python 3 interpreter.

Additional checks: editor image rendered and visually inspected; stereo anti-phase and block-partition invariance; NaN, Inf and extreme finite input; confidence/target meters; 0/50/100% strength; mono/stereo layouts; saved-state round trip; host bypass. The unchanged shifter's nominal latency remains 992 samples at 48 kHz.

## Pitch accuracy and octave errors

Each stationary family uses 12 frequencies (65, 70, 82.41, 110, 146.83, 220, 261.63, 329.63, 440, 659.25, 880, 1000 Hz), three rates (44.1/48/96 kHz), and 0.55-second fixtures. Scoring starts at 150 ms: 1,440 frames per family. Waveforms use deterministic phase accumulation and harmonic amplitudes pure [1], rich [1,.6,.3,.15], dominant second [.08,1,.3,.2], missing fundamental [0,.8,.6,.3], scaled by 0.2.

Errors are absolute 1200 log2(estimate/reference), scored only on valid frames. Octave errors are absolute errors within 100 cents of 1200; gross errors are all errors over 600 cents, also exported separately so non-octave failures cannot hide. All four stationary families have **zero raw and tracked gross errors**, 100% scored raw/tracked recall, 100% target accuracy and zero target switches. Targets use C major and the known input frequency.

| Fixture | Raw valid / frames | Median cents | P95 cents | Max cents | Tracked P95 | Raw / tracked octave errors |
|---|---:|---:|---:|---:|---:|---:|
| pure | 1440/1440 | 0.34 | 5.62 | 6.78 | 5.62 | 0 / 0 |
| harmonic | 1440/1440 | 0.33 | 5.29 | 6.75 | 5.29 | 0 / 0 |
| dominant_second | 1440/1440 | 0.13 | 4.51 | 5.78 | 4.51 | 0 / 0 |
| missing_fundamental | 1440/1440 | 0.25 | 4.28 | 5.69 | 4.28 | 0 / 0 |

### Comparison with the original detector

The baseline detector was extracted directly from commit cb87454 and compiled with MinGW solely for this pure-C++ numerical comparison. No JUCE/MinGW plugin build is claimed. Both revisions use the same stationary fixtures. tests/DetectorComparison.cpp can be compiled with its include directory pointing at either Source/dsp or a directory containing that extracted header; the regular CMake detector_comparison target uses the current header.

| Family | M1 raw valid / 1440 | M1 P95 cents | M1 gross errors | M2 raw valid / 1440 | M2 gross errors |
|---|---:|---:|---:|---:|---:|
| Pure | 1320 | 2.44 | 0 | 1440 | 0 |
| Harmonic | 1318 | 2.80 | 0 | 1440 | 0 |
| Dominant second | 1320 | 1208.56 | 416 | 1440 | 0 |
| Missing fundamental | 1358 | 3.18 | 0 | 1440 | 0 |

The improvement is coverage and fewer harmonic/octave mistakes, not a universal decrease in sub-cent error. M2's cheaper analysis rate and endpoint coverage have higher P95 clean-tone error (up to 5.62 cents here); M1's conditional error excludes rejected endpoint frames. Keep both recall and error when comparing revisions.

## Noise robustness

Deterministic zero-mean uniform broadband noise at nominal pre-analysis SNRs is mixed with harmonics [1,.6,.3]. Frequencies 110/220/440/880 Hz at three rates, 0.65 seconds each, scored after 150 ms: 600 frames per SNR. Filtering improves effective low-band SNR, so these values are not equivalent to arbitrary real microphone noise.

| Fixture | Raw valid / frames | Median cents | P95 cents | Max cents | Tracked P95 | Raw / tracked octave errors |
|---|---:|---:|---:|---:|---:|---:|
| noise_snr_30.000000 | 600/600 | 0.72 | 1.41 | 1.64 | 1.41 | 0 / 0 |
| noise_snr_20.000000 | 600/600 | 0.73 | 1.51 | 1.83 | 1.49 | 0 / 0 |
| noise_snr_10.000000 | 600/600 | 1.07 | 2.71 | 4.70 | 2.57 | 0 / 0 |
| noise_snr_0.000000 | 600/600 | 3.58 | 16.22 | 34.00 | 15.07 | 0 / 0 |
| noise_snr_-10.000000 | 0/600 | N/A | N/A | N/A | N/A | 0 / 0 |

At 0 dB, 594/600 frames are in Tracking; six are temporarily unsupported. Mean raw confidence is about 0.90, so correction is attenuated on weaker frames. At -10 dB, all raw frames are rejected and confidence averages about 0.52. N/A means no accepted pitch, not zero error. Final fixtures have zero raw/tracked octave and gross errors at all tested SNRs; this does not cover all random seeds, noise spectra or singing styles.

## Voicing

A 48 kHz sequence of silence → 220 Hz harmonics → broadband noise → harmonics → silence uses five 400 ms segments. Precision/recall exclude the first 80 ms after each boundary; transition response is measured separately rather than hidden inside those stationary statistics.

| Measurement | Result |
|---|---:|
| True voiced / false voiced | 64 / 0 |
| True unvoiced / missed voiced | 96 / 0 |
| Precision / recall | 1.00 / 1.00 |
| Voiced→noise decision | 20 ms |
| Target reacquisition after noise | 30 ms |
| High-pass-noise sibilance proxy | 0 / 40 frames voiced |
| Amplitudes 0.0001 / 0.0005 | 0 / 35 voiced frames each |
| Amplitudes 0.004 / 0.9 | 35 / 35 voiced frames each |

The sibilance proxy is synthetic high-pass noise, not a recorded S/SH. The amplitude numbers scale the harmonic waveform and are not RMS values. Periodic hum is still expected to be classified as pitched. Genuine breathy/creaky voice remains an open validation requirement.

## Vibrato, slides and target hysteresis

Vibrato is 5 Hz sinusoidal modulation, +/-40 cents about A4, over two seconds at three rates. All 555 scored frames retain target A4, with zero switches and zero octave/gross errors. Tracked causal P95 error is at most 22.28 cents: the tracker follows motion with finite analysis lag, not an artificially time-aligned reference.

Two-second C4→G4 and A3→A4 glides have no gross/octave errors, full scored tracking coverage, and tracked P95 errors below 12 cents at each rate. Target changes during a glide are expected; they are not counted as stable-note failures.

Unit trajectories prove C→midpoint→D and D→midpoint→C switch only after crossing the 20-cent hysteresis margin for two consecutive frames. One hundred +/-10-cent midpoint oscillations remain on the existing target from either direction. Additional tests cover key/scale invalidation, isolated +/-octave and five-semitone errors, a genuine four-frame octave transition, acquisition, uncertain frames and stale-pitch expiry.

## Control-response latency

| Event | Measured clean synthetic result |
|---|---:|
| First valid raw frame | 49.89–50.00 ms |
| First musical target | 59.86–60.00 ms |
| C4→D4 target change | about 30–39 ms |
| E4→G4 target change | about 39–40 ms |
| A3→A4 target change | about 69–70 ms |

The cents smoother then reaches 63% of a step in 25 ms or 90% in about 58 ms. Initial target plus 90% smoothing is roughly 118 ms on these fixtures. A target switch is not the time when all shifted audio sounds fully settled. Audio buffer delay remains separate: nominal 20.67 ms at 48 kHz with variable granular delay while shifting. No extra lookahead audio buffer was added.

## CPU performance and real-time review

Three seconds per configuration, stereo core including analysis, tracking and the unchanged shifter. The benchmark times only process(), not signal generation or report formatting. All complete blocks are included; analysis bursts and startup blocks are retained. Values below are one measured run, not a hard real-time guarantee.

| Rate | Block | Mean ms | P95 ms | Max ms | Budget ms | Max / budget |
|---:|---:|---:|---:|---:|---:|---:|
| 44100 | 32 | 0.0038 | 0.0352 | 0.1674 | 0.726 | 23.1% |
| 44100 | 64 | 0.0072 | 0.0363 | 0.1153 | 1.451 | 7.9% |
| 44100 | 128 | 0.0145 | 0.0409 | 0.0875 | 2.902 | 3.0% |
| 44100 | 256 | 0.0290 | 0.0449 | 0.0805 | 5.805 | 1.4% |
| 44100 | 512 | 0.0586 | 0.0867 | 0.1500 | 11.610 | 1.3% |
| 48000 | 32 | 0.0038 | 0.0413 | 0.0634 | 0.667 | 9.5% |
| 48000 | 64 | 0.0077 | 0.0425 | 0.1246 | 1.333 | 9.3% |
| 48000 | 128 | 0.0154 | 0.0455 | 0.1428 | 2.667 | 5.4% |
| 48000 | 256 | 0.0293 | 0.0487 | 0.0689 | 5.333 | 1.3% |
| 48000 | 512 | 0.0593 | 0.0953 | 0.1417 | 10.667 | 1.3% |
| 96000 | 32 | 0.0024 | 0.0013 | 0.0607 | 0.333 | 18.2% |
| 96000 | 64 | 0.0050 | 0.0415 | 0.1515 | 0.667 | 22.7% |
| 96000 | 128 | 0.0098 | 0.0437 | 0.1520 | 1.333 | 11.4% |
| 96000 | 256 | 0.0195 | 0.0482 | 0.1046 | 2.667 | 3.9% |
| 96000 | 512 | 0.0395 | 0.0622 | 0.1627 | 5.333 | 3.1% |

Mean callback costs are small; the largest sampled callback used about 23.1% of its block budget in this final run. OS scheduling can produce different maxima. Earlier pre-optimization runs reached 67.3% at a small buffer. The final detector uses prefix-summed window energy and decimates 44.1 kHz to 11.025 kHz to reduce per-hop work. Both changes were followed by regression tests. No repeated-run minimum was selected for this table.

The allocation test observed **zero C++ new/new[] calls**, including aligned forms, during 3,000 variable-sized core processing calls with changing parameters at three rates. Buffers allocate only in prepare. Review found no locks, I/O, GUI calls, network use or string construction in the callback path. This is bounded offline evidence, not proof about every host/device/OS condition.

## Plugin, host and offline integration

Release VST3 0.2.0 builds and loads in the JUCE binary host test, creates its native editor and shifts the 432 Hz fixture toward A4 under the existing 20-cent tolerance. Processor tests confirm diagnostic publication, state recall, bypass and matching mono/stereo layouts. The wider diagnostic text fits in the 500x410 editor.

The WAV tool uses the same TrackingPipeline. Python integration tests check WAV decoding at three rates, anti-phase stereo left-channel selection, all three strengths, monotonic approximately 10 ms timestamps, CSV pitch/target/confidence semantics, reference statistics and invalid input handling. The final CSV row is the last completed analysis hop, not a fabricated exact end-of-file frame.

REAPER 7.57 recognized VocalPilot in a fresh isolated VST scan configuration. The VST3 class ID remains ABCDEF019182FAEB56706A7456703031. Track playback, interactive diagnostics in REAPER and real singing remain **manual acceptance work**; the startup/evaluation screen again prevented unattended project rendering. No human vocal recording was used or downloaded.

## Regressions and intentional behavior changes

All original Milestone 1 tests pass. Intentional changes: uncertain/raw frames no longer directly set correction; target switching has hysteresis; confidence scales correction; acquisition is shorter; pathological finite audio above +/-16 is bounded; the nominal pitch range has 0.5% interpolation tolerance; and the UI exposes both raw and tracked states. Bypass, nominal latency, key/scale definitions, parameter IDs and ordinary 0% dry behavior are preserved. The shifter source was not changed.

Development failures were resolved before this report: near-global integer-lag scoring chose subharmonics at high missing-fundamental pitches; interpolation and a documented candidate tolerance corrected selection. A later analysis-rate optimization required interpolating confidence as well. A CSV test incorrectly assumed the final frame must occur exactly at the end of the WAV; it now checks the completed-hop timing contract. No failed acceptance test is suppressed.

## Known limits and next manual step

No real-human-vocal accuracy, formant preservation, commercial artifact suppression, other-platform build, broad plugin-validator pass or guaranteed hard real-time performance is claimed. Weak odd harmonics, ambiguous missing fundamentals, breathy/creaky singing, changing SNR, vibrato spanning both hysteresis thresholds and rapid or repeated confidently wrong octave estimates remain risks. Fixed gates do not adapt to every recording chain.

Follow [REAPER-MANUAL-TEST.md](REAPER-MANUAL-TEST.md) for the exact own-vocal procedure and failure-stage diagnosis. Use [OFFLINE-ANALYSIS.md](OFFLINE-ANALYSIS.md) to add permission-cleared recordings and independent pitch labels. Milestone 3 has not been started.

## Reproduce

Configure the normal build with -DVOCALPILOT_HOST_TEST=ON and a Python 3 interpreter available (or -DPython3_EXECUTABLE=your/python). Build Release, then run ctest --test-dir build -C Release --output-on-failure. Run tracking_tests and tracking_benchmark to reproduce the tables. All generated benchmark files and sanitized test logs are retained alongside this report; timing will vary by machine and load.
