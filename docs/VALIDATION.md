# Milestone 1 validation

## Verified on this machine

Windows x64, MSVC 19.44 (toolset 14.44.35207), Windows SDK 10.0.26100.0, CMake 3.31.8, Ninja 1.12.1, JUCE 8.0.12, Release build.

- VST3 bundle and module manifest built successfully.
- All three CTest suites pass: DSP tests, JUCE processor/editor tests, and a real VST3 binary host test.
- Synthetic harmonic tones at 70, 110, 220, 432 and 880 Hz, at 44.1/48/96/192 kHz, detected within 1.5 cents in these fixtures. This does not establish equivalent accuracy for all voices.
- Flat (432 Hz) and sharp (448 Hz) sine tones shifted toward A4; measured outputs about 439.87–440.52 Hz across those sample rates.
- The separate JUCE host loaded the actual compiled VST3, created its editor, reported 992 samples latency and measured **440.283 Hz** from a 432 Hz input at 48 kHz.
- An independent NumPy FFT of the saved VST3 output found its dominant spectral peak at **440.094 Hz**, confirming the shift without relying on the plugin's pitch detector.
- Bypass from preparation and zero strength produced exact input samples after the reported delay. Stereo anti-phase inputs stayed anti-phase. Variable block sizes from 1 to 511 samples, silence, noise rejection, and non-finite input were exercised.
- All keys/scales produce permitted targets, with explicit tests for octave boundaries, exact ties, natural minor, and changing key during processing. Strength 50% produces half the requested cents correction.
- Mono/stereo processor layouts, finite output, host bypass, all four saved parameter values, and editor construction passed.
- The editor was rendered to an image and visually inspected.
- Representative 256-sample/48 kHz callback measurements: mean about **0.048 ms**, maximum **0.172 ms** across the sampled mono/stereo test blocks, versus a **5.333 ms** block duration. This short offline measurement is not a hard real-time guarantee or worst-case OS scheduling test.

`test-results.txt` contains the final verbose CTest output. `examples/corrected-432-to-A4.wav` was generated through the compiled VST3 host, not by post-processing the source tone in another application.

Delivered x64 binary SHA-256: `D54850284891988B8D6AB40F53D077607EC57B688551F16AC93E7A6A29ED2FB5`.

## REAPER status

REAPER 7.57's VST3 scan successfully recognized the bundle as **VocalPilot (VocalPilot)** and recorded its VST3 class ID. An isolated REAPER configuration was used, without modifying the user's normal configuration.

The subsequent scripted project/render test stalled at REAPER startup and produced no render. Desktop window control was unavailable in this session. Therefore **actual REAPER track playback, native editor interaction in REAPER, and live singing are not yet verified**. Successful scanning and loading in the JUCE test host are useful evidence but do not substitute for this acceptance step.

## Remaining manual acceptance test

1. Add `bin` to REAPER's VST paths and scan. Load `examples/REAPER-Smoke.lua` through Actions → Show action list → New action → Load ReaScript, then run it. It creates a new project tab with a 432 Hz tone and VocalPilot, leaving existing project tabs intact. Press Play, observe approximately A4 and +31 cents correction, then toggle bypass to hear 432 Hz.
2. Record or route a dry single vocal to the track. Sustain a note for at least half a second. Compare the meter with REAPER's tuner; test both slightly sharp and flat notes.
3. Change C major to D major around C#; C# becomes permitted. Change C major to C minor around E-flat; E-flat becomes permitted. Avoid exact scale boundaries when judging stability.
4. Sweep strength, toggle bypass, save/reopen the project, and confirm settings persist. Verify mono and stereo routing and both standard and small device buffers.
5. Listen for noise bursts, clicks, dropouts and excessive grain beating. Watch REAPER's performance meter during several minutes of singing, including consonants and breath. Expected grain coloration is a prototype limitation; overloads or invalid output are failures.

No recorded-vocal listening test, broad plugin-validator suite, other host, macOS/Linux build, or production stability claim is included in this milestone's automated evidence.
