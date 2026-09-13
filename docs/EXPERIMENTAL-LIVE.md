# Experimental Live: installation and listening test

The new **HQ / LIVE (experimental)** selector preserves HQ as the default. Old
sessions without a mode parameter load in HQ. Live runs only the asymmetric
pitch-synchronous engine; unused HQ/spectral engines are not processed. Change
mode while stopped: processing resets briefly and the host latency updates.
Mode is saved in sessions but is not automatable.

## Supported first test

- **48 kHz, 128-sample interface buffer, 140–500 Hz source fundamentals** (roughly
  C#3–B4), corrections up to **±200 cents**.
- A small detector tolerance of 139–503 Hz prevents measurement error at the
  boundaries from disabling correction. This is not an additional validated range.
- Unvoiced, poor-confidence, unsupported pitches and excessive correction requests
  return toward delayed dry audio. Low rap/spoken passages may remain untuned.
- Live reports **864 samples / 18 ms** at 48 kHz. Dry/bypass transport is exactly
  864 samples. Wet timing varies with donor epochs: this is a conservative host
  budget, **not sample-exact wet/dry alignment**. Interface and other FX latency
  are additional. Parallel dry monitoring can produce comb filtering.
- M2 tracking and its acquisition/25 ms correction smoothing remain unchanged.
  Tuning reaction time is separate from the audio transport delay.
- Other sample rates scale timing but have not been validated in this prototype.

## Replace your installed VST3

1. Close REAPER and any other host using VocalPilot.
2. The installed bundle found on this machine is:
   `C:\Program Files\Common Files\VST3\VocalPilot\bin\VocalPilot.vst3`
3. Back up that **entire folder** outside all plugin scan directories, for example
   `C:\Users\Lorenzo\Documents\Codex\VocalPilot-Plugin-Backups\Before-Live\VocalPilot.vst3`.
4. After backing it up, replace the installed bundle with the entire new folder:
   `C:\Users\Lorenzo\Documents\Codex\VocalPilot\bin\VocalPilot.vst3`.
   Copy it into the existing `...\VocalPilot\bin\` directory. Windows may request
   administrator permission. Copy the bundle, not the whole project or just the
   inner binary. Do not leave a second VocalPilot bundle in the scan path.
5. Reopen REAPER. In Preferences > Plug-ins > VST, re-scan. If the old UI persists,
   clear the VST cache/re-scan, then remove and reinsert VocalPilot. Check for
   duplicate installations if the mode selector is missing.
6. Select LIVE while stopped. At 48 kHz the UI must show **864 samples** and the
   HQ engine dropdown becomes disabled. If compensation does not refresh, remove
   and reinsert the FX.

Rollback: close REAPER, replace the bundle with the backup, and re-scan.
This development task did not replace your installed plugin automatically.

## Listen and record

1. Select your interface's ASIO driver, 48 kHz and a 128-sample buffer. Use headphones.
2. Create a track, choose the microphone's mono hardware input, record-arm it, and
   enable input monitoring. Disable interface direct monitoring. Start without
   other FX that add latency.
3. Insert VocalPilot as a normal track FX. Select LIVE, your key/scale, Strength
   100%, Bypass off. Start with comfortable notes inside the supported range.
4. Sustain a slightly sharp/flat note. Compare bypass for perceived delay, then
   listen for correction. Try ah → oh, slides, hard note transitions, and your
   normal singing/rap delivery. Note metallic, grainy, robotic, warbly, pumping,
   or unexpectedly untuned passages and the displayed frequency at those moments.
5. Record a short take. Normal track-FX monitoring lets you hear Live while
   recording the input dry, so you can revisit the processing. If you deliberately
   want printed tuning, use the track's Record: output (mono) option; keep a dry
   recording too if you want a recoverable original.
6. Compare sound with HQ / Pitch synchronous during playback. HQ retains its
   original ~100 ms latency at 48 kHz, so expect a different live-monitoring feel.

For feedback, record interface/buffer settings, range, key/scale/strength, whether
delay or correction response bothers you, and which gestures sound bad. The next
engineering task should follow your listening feedback, not more optimization.

## Architecture and validation

`Source/dsp/transformation/LivePitchSynchronous.h` is separate from HQ. It uses a
one-period left window and quarter-period right window, completed past epochs,
and an independently spaced synthesis lattice. Internal synthesis offset: 10 ms.
Grains beyond the 18 ms transport budget are rejected. Dry uses the host budget.
`CorrectionEngine` routes to Live or the existing HQ bank. Processor mode changes
allocate/reset on the message thread under the callback lock, not in processBlock.
Brief interruption on mode changes is expected; do not switch while performing.

The 19 direct-engine cases cover 140/180/220/350/500 Hz at +25/+100/-100 cents and
±200 cents at the two boundaries. They passed finite-output, steady coverage and
pitch checks. Measured markers: **404–817 samples / 8.42–17.02 ms**, below the
864-sample budget. These are a few marker positions, not an exhaustive guarantee
for every changing vocal. Direct-engine pitch errors were under 1.52 cents with
the existing detector. No real-vocal listening result is claimed.

M2+Live corrected 432 Hz to approximately 440.29 Hz, 140 Hz to 146.84 Hz, and
500 Hz to 494.26 Hz (target 493.88 Hz). Mean stereo 128-block processing was about
0.026 ms in the short local run; this is not a worst-case CPU guarantee. Exact
zero-correction/unsupported dry, stereo, state roundtrip, old-state HQ default,
message-thread switching and reported latency passed. M2 source files are unchanged.

**7/7 essential suites passed:** DSP, tracking, real-time safety, transformation,
processor/state/editor, Live smoke, and VST3 binary smoke. The actual VST3 loaded
in the JUCE smoke host and processed audio in Live plus all three HQ engines.
REAPER microphone monitoring remains your listening evaluation, not a claimed test.
See EXPERIMENTAL-LIVE-VALIDATION.txt for the captured results.

Build with `..\VocalPilot-DevTools\build.ps1 -SkipTests`, then run:

```powershell
& ..\VocalPilot-DevTools\cmake-3.31.8-windows-x86_64\bin\ctest.exe --test-dir build -C Release --output-on-failure -R '^(dsp_tests|tracking_tests|realtime_safety_tests|transformation_tests|processor_tests|live_smoke|vst3_smoke)$'
```

No large quality suite or failed dual-read-head development was performed.
