# VocalPilot — Experimental Live + Milestone 3 HQ

C++17 / JUCE VST3 monophonic vocal pitch correction for REAPER. This build adds an experimental Live monitoring path while preserving the Milestone 3 HQ engines and all M2 tracking, targeting and 25 ms correction smoothing.

**This is a listening prototype, not completed sonic acceptance.** The actual VST3 has passed automated host loading and processing checks. Real microphone monitoring in REAPER is the next evaluation; further DSP work should follow listening feedback.

## New: experimental Live mode

Choose **HQ / LIVE (experimental)** in the editor. Change mode while stopped: switching resets processing and updates host latency. The choice is saved with the session but is not automatable. HQ remains the default, including for old sessions without a mode setting.

| | LIVE (experimental) | HQ |
|---|---|---|
| Transformation | Causal/asymmetric pitch-synchronous overlap-add only | Legacy reference, Pitch synchronous, Spectral phase locked |
| First listening setup | 48 kHz, 128-sample interface buffer | Existing Milestone 3 playback/comparison workflow |
| Intended Live source range | 140–500 Hz, correction up to ±200 cents | Existing HQ behavior retained |
| Host latency at 48 kHz | 864 samples / 18 ms conservative budget | 4800 samples / 100 ms common alignment |

Live does not run unused HQ/spectral engines in parallel. Unvoiced, poor-confidence, out-of-range or excessive-correction input returns toward delayed dry audio. Low spoken/rap passages may therefore remain untuned.

**Live wet timing is variable, not sample-exact with dry.** Tested wet markers arrived at 404–817 samples (8.42–17.02 ms); the dry path uses the full reported 864 samples. These limited measurements are not an exhaustive guarantee for changing vocals. Interface latency and other FX are additional. M2 acquisition and retune response are unchanged and separate from audio transport latency. Other sample rates are not validated for this first Live listening pass.

See [installation, rollback, limitations and REAPER listening instructions](docs/EXPERIMENTAL-LIVE.md) and the [captured validation results](docs/EXPERIMENTAL-LIVE-VALIDATION.txt). The failed dual-read-head experiment is not part of Live.

## Build

Use CMake 3.22+, Git, Visual Studio 2022 C++ Build Tools and a Windows SDK. JUCE 8 does not support MinGW. JUCE is pinned to 8.0.12, commit `29396c22c93392d6738e021b83196283d6e4d850`. From a developer terminal:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVOCALPILOT_HOST_TEST=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure -R "^(dsp_tests|tracking_tests|realtime_safety_tests|transformation_tests|processor_tests|live_smoke|vst3_smoke)$"
```

Use `-DJUCE_DIR=C:/path/to/JUCE` for a local checkout. Otherwise CMake fetches the pinned revision. Ninja/MSVC uses `-G Ninja -DCMAKE_BUILD_TYPE=Release`. macOS/Linux builds are not validated here. Copy the whole `build/VocalPilot_artefacts/Release/VST3/VocalPilot.vst3` bundle. No system-wide installation occurs automatically. Other PCs may need the Microsoft Visual C++ 2015–2022 x64 runtime.

The command above runs the seven essential suites used for this Live build; all seven passed. It includes Live range/coverage/pitch checks, exact zero-correction and unsupported dry output, M2 integration, state compatibility, mode switching, and actual VST3 loading with Live plus all three HQ engines. It does not run the large Milestone 3 benchmark campaign.

The complete validation configuration includes Python 3 and NumPy for independent spectral measurements. Set `-DPython3_EXECUTABLE=your/python` if needed; install offline test dependencies with `python -m pip install -r tools/requirements-quality.txt`. They are not shipping plugin dependencies. Without NumPy the numerical quality regression is omitted explicitly by CMake; without Python the WAV harnesses are omitted. With both and the optional host enabled there are eleven CTest suites. Omit the `-R` filter to run all configured suites when needed.

Pure C++ tests can build without JUCE:

```powershell
cmake -S . -B build-dsp -DVOCALPILOT_DSP_ONLY=ON
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release --output-on-failure
```

## Use in REAPER

Close REAPER before replacing the whole VST3 bundle, then reopen and rescan. For monitoring, select your interface's ASIO driver at 48 kHz / 128 samples, arm a mono microphone track, enable input monitoring, and turn off interface direct monitoring. Insert VocalPilot as a track FX, select LIVE while stopped, and choose your key/major or natural-minor scale. Start at Strength 100% inside the supported range. Confirm the editor reports 864 samples at 48 kHz.

Listen to sustained notes, ah → oh transitions, slides and hard note transitions. Note delayed response, metallic/grainy/robotic/warbly sound, and untuned passages. Normal track-FX monitoring preserves a dry input recording for later comparison. Use the [full listening instructions](docs/EXPERIMENTAL-LIVE.md) for installation paths, printed recording and rollback.

Stereo detection uses the left channel; both channels share transformation decisions. Strength scales the correction interval, not retune speed. Bypass/zero strength return delayed dry after transition. MIDI, formant controls, polyphony and graphical editing are not implemented.

HQ audio delay is 4608 samples at 44.1 kHz (104.49 ms), 4800 at 48 kHz (100 ms), and 9600 at 96 kHz (100 ms), regardless of selected engine. REAPER receives that exact common delay. Legacy's moving grain positions still make its instantaneous shifted delay variable. Detection/tracker acquisition and retune response are additional control behavior, not extra host-reported audio latency. Spectral cold-start correction waits for complete window coverage. Start with playback at 48 kHz/256 samples; this is not a low-latency monitoring mode.

Follow the [Milestone 3 own-vocal/listening procedure](docs/MILESTONE3-LISTENING.md). It covers vowels, low/high notes, sharp/flat corrections, vibrato, slides, octave changes, breath and consonants, with explicit failure diagnosis.

## Offline comparisons

```powershell
.\build\vocalpilot_render_artefacts\Release\vocalpilot_render.exe dry-vocal.wav test-output\take-01 --key 0 --strength 100
python tools/blind_listening.py prepare test-output/take-01 test-output/listen-01
```

The renderer remains the Milestone 3 comparison tool; it does not render the new Live mode. It computes one causal M2 trajectory and reuses it for dry/legacy/synchronous/spectral aligned WAVs. It exports controls.csv for sample-exact replay and rejects existing destination folders. A separate fixed-F0 or explicit-trajectory mode isolates transformation quality. See the [workflow](docs/MILESTONE3-LISTENING.md) before using oracle controls on real recordings.

The original `vocalpilot_analyse` WAV-to-CSV utility remains available; see [M2 analysis documentation](docs/OFFLINE-ANALYSIS.md). Private vocal fixtures are ignored under `tests/audio/vocals/`; its README and metadata template explain permissions. No human corpus is included.

## Structure and evidence

- `Source/dsp/`: M2 tracking/control and legacy shifter.
- `Source/dsp/transformation/`: common control contract, delay history, FFT, original HQ engines/bank, and separate `LivePitchSynchronous.h`.
- `Source/Plugin*`: JUCE host integration, six saved parameters including mode, and diagnostic UI.
- `tools/`: aligned renderer, blind ratings, independent NumPy quality/event measurements and original CSV analyser.
- `tests/`: historical regressions, transformation invariants, allocation/CPU probes, WAV harnesses, `LiveSmoke.cpp` and binary VST3 hosting.
- `docs/`: [architecture](docs/MILESTONE3-ARCHITECTURE.md), [measured results and limitations](docs/MILESTONE3-VALIDATION.md), historical M1/M2 evidence and manual procedures.

Historical research tools remain available when explicitly needed: `transformation_benchmark` covers three rates and five block sizes; `python tools/transformation_quality.py path/to/vocalpilot_render.exe test-output/quality` runs the full 372-fixture vowel sweep; `python tools/transformation_events.py path/to/vocalpilot_render.exe test-output/events` runs dynamic/transient/Nyquist probes. These are not required for the current Live listening evaluation. Numerical tests measure synthetic behavior; they do not produce listening scores or validate commercial quality.

Build outputs, `bin/`, local audio, IDE caches and private configuration remain ignored. Pinned JUCE licensing remains applicable; see [JUCE notice](docs/JUCE-LICENSE.md). No external pitch library has been added as a shipping dependency.
