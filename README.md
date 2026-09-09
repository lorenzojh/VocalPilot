# VocalPilot — Milestone 3 research prototype

C++17 / JUCE VST3 monophonic vocal pitch correction. Milestone 3 adds two original transformation candidates alongside the legacy shifter: pitch-synchronous waveform overlap-add and a phase-locked spectral engine with envelope restoration. M2 tracking, targeting and 25 ms correction smoothing remain unchanged.

**This is an engineering bake-off, not completed sonic acceptance.** Synthetic tests and an actual VST3 host are available. Real-vocal listening and REAPER playback are still required. Legacy remains the default until that comparison is reviewed; select either new engine in the UI.

## Build

Use CMake 3.22+, Git, Visual Studio 2022 C++ Build Tools and a Windows SDK. JUCE 8 does not support MinGW. JUCE is pinned to 8.0.12, commit `29396c22c93392d6738e021b83196283d6e4d850`. From a developer terminal:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVOCALPILOT_HOST_TEST=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Use `-DJUCE_DIR=C:/path/to/JUCE` for a local checkout. Otherwise CMake fetches the pinned revision. Ninja/MSVC uses `-G Ninja -DCMAKE_BUILD_TYPE=Release`. macOS/Linux builds are not validated here. Copy the whole `build/VocalPilot_artefacts/Release/VST3/VocalPilot.vst3` bundle. No system-wide installation occurs automatically. Other PCs may need the Microsoft Visual C++ 2015–2022 x64 runtime.

The complete validation configuration includes Python 3 and NumPy for independent spectral measurements. Set `-DPython3_EXECUTABLE=your/python` if needed; install offline test dependencies with `python -m pip install -r tools/requirements-quality.txt`. They are not shipping plugin dependencies. Without NumPy the numerical quality regression is omitted explicitly by CMake; without Python the WAV harnesses are omitted. With both and the optional host enabled there are ten CTest suites.

Pure C++ tests can build without JUCE:

```powershell
cmake -S . -B build-dsp -DVOCALPILOT_DSP_ONLY=ON
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release --output-on-failure
```

## Use in REAPER

Rescan the bundle, insert VocalPilot on one dry vocal, choose key/major or natural-minor scale, and compare the three engine choices. Stereo detection uses the left channel; both channels share transformation decisions. Strength scales the correction interval, not retune speed. Bypass/zero strength return aligned dry after transition. MIDI, formant controls, polyphony and graphical editing are not implemented.

HQ audio delay is 4608 samples at 44.1 kHz (104.49 ms), 4800 at 48 kHz (100 ms), and 9600 at 96 kHz (100 ms), regardless of selected engine. REAPER receives that exact common delay. Legacy's moving grain positions still make its instantaneous shifted delay variable. Detection/tracker acquisition and retune response are additional control behavior, not extra host-reported audio latency. Spectral cold-start correction waits for complete window coverage. Start with playback at 48 kHz/256 samples; this is not a low-latency monitoring mode.

Follow the [Milestone 3 own-vocal/listening procedure](docs/MILESTONE3-LISTENING.md). It covers vowels, low/high notes, sharp/flat corrections, vibrato, slides, octave changes, breath and consonants, with explicit failure diagnosis.

## Offline comparisons

```powershell
.\build\vocalpilot_render_artefacts\Release\vocalpilot_render.exe dry-vocal.wav test-output\take-01 --key 0 --strength 100
python tools/blind_listening.py prepare test-output/take-01 test-output/listen-01
```

The renderer computes one causal M2 trajectory and reuses it for dry/legacy/synchronous/spectral aligned WAVs. It exports controls.csv for sample-exact replay and rejects existing destination folders. A separate fixed-F0 or explicit-trajectory mode isolates transformation quality. See the [workflow](docs/MILESTONE3-LISTENING.md) before using oracle controls on real recordings.

The original `vocalpilot_analyse` WAV-to-CSV utility remains available; see [M2 analysis documentation](docs/OFFLINE-ANALYSIS.md). Private vocal fixtures are ignored under `tests/audio/vocals/`; its README and metadata template explain permissions. No human corpus is included.

## Structure and evidence

- `Source/dsp/`: M2 tracking/control and legacy shifter.
- `Source/dsp/transformation/`: common control contract, delay history, FFT, two new engines and the comparison bank.
- `Source/Plugin*`: JUCE host integration, five saved parameters and diagnostic UI.
- `tools/`: aligned renderer, blind ratings, independent NumPy quality/event measurements and original CSV analyser.
- `tests/`: historical regressions, transformation invariants, allocation/CPU probes, WAV harnesses and binary VST3 hosting.
- `docs/`: [architecture](docs/MILESTONE3-ARCHITECTURE.md), [measured results and limitations](docs/MILESTONE3-VALIDATION.md), historical M1/M2 evidence and manual procedures.

Run `transformation_benchmark` for all three rates and five block sizes. Run `python tools/transformation_quality.py path/to/vocalpilot_render.exe test-output/quality` for the full 372-fixture vowel sweep, and `python tools/transformation_events.py path/to/vocalpilot_render.exe test-output/events` for dynamic/transient/Nyquist probes. The numerical tests measure synthetic behavior; they do not produce listening scores or validate commercial quality.

Build outputs, `bin/`, local audio, IDE caches and private configuration remain ignored. Pinned JUCE licensing remains applicable; see [JUCE notice](docs/JUCE-LICENSE.md). No external pitch library has been added as a shipping dependency.
