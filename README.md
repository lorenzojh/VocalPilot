# VocalPilot — Milestone 1

A small C++17 / JUCE VST3 monophonic vocal-correction prototype. Controls: chromatic key, major/natural-minor scale, correction strength (0–100%), and bypass. The editor shows frequency, fractional MIDI note, nearest chromatic note name, cents deviation, target note, and smoothed correction in cents. A4 = 440 Hz; MIDI 60 = C4.

## Build

Use CMake 3.22+, Git, and a supported desktop C++ compiler. On Windows install Visual Studio 2022 Build Tools with **Desktop development with C++** and a Windows SDK. JUCE 8 does **not** support MinGW. The dependency is pinned to JUCE 8.0.12, commit `29396c22c93392d6738e021b83196283d6e4d850`.

From this directory in a developer terminal:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

For a local JUCE checkout, append `-DJUCE_DIR=C:/path/to/JUCE` to configure. Otherwise CMake downloads the pinned revision. A supported Ninja/MSVC developer terminal can use `-G Ninja -DCMAKE_BUILD_TYPE=Release`. macOS/Linux may use their normal CMake generator and JUCE platform dependencies; they have not been validated here.

The bundle is `build/VocalPilot_artefacts/Release/VST3/VocalPilot.vst3`. Copy the **whole bundle folder**, not just its inner binary. No plugin is automatically installed into a system directory.

The milestone distribution ZIP includes the Windows x64 binary in `bin/VocalPilot.vst3`; `bin/` is intentionally excluded from Git, so a source checkout must build it first. Other Windows PCs may need the Microsoft Visual C++ 2015–2022 x64 runtime. Add `-DVOCALPILOT_HOST_TEST=ON` at configure time to build an additional test that loads the actual VST3 binary, creates its editor and processes a tone through its host-facing interface.

DSP tests can run without JUCE:

```sh
cmake -S . -B build-dsp -DVOCALPILOT_DSP_ONLY=ON
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release --output-on-failure
```

## REAPER

1. Open Preferences → Plug-ins → VST. Add the directory containing `VocalPilot.vst3` to the scan paths and re-scan.
2. Insert **VST3: VocalPilot (VocalPilot)** on a vocal track. Choose key and scale; start at 100% strength.
3. Use a dry single voice. Both mono/mono and stereo/stereo buses are supported. For stereo, the **left channel** drives detection and the same shift is applied to both channels. Route a right-only vocal to the left first.
4. Sustain a note slightly flat or sharp. The detector needs about 85–95 ms to acquire a stable pitch at common sample rates. Check that the target and correction sign are sensible: positive cents raises pitch, negative lowers it.
5. Compare against bypass and 0% strength. Strength scales the pitch interval; it is not a wet/dry control and does not adjust the fixed 25 ms retune smoothing time.

The plugin reports `32 + floor(0.040 * sampleRate)/2` samples of nominal latency (992 samples / 20.67 ms at 48 kHz). Bypass and 0% strength use an exact fixed-delay dry path after a short transition. Host bypass is handled too. The granular shifter has variable instantaneous delay, so host compensation is approximate while shifting. Pitch-detection acquisition time is additional control response time, not a delay added to the audio buffer. Expect this to be more suitable for prototype playback experiments than polished live monitoring.

## Scope and limitations

- Approximately 65–1000 Hz detection, fixed voicing threshold and energy gate; breath, noise, weak fundamentals and fast transitions can cause missed or octave-wrong estimates.
- Basic time-domain shifting produces coloration, grain beating and transient smearing. No formant preservation. Stationary unity uses the dry path to avoid two-tap comb filtering.
- No target hysteresis: singing exactly between valid notes may switch targets. Exact distance ties choose the lower note.
- Fixed-size analysis storage; delay buffers allocate only in preparation. Audio processing uses no locks, file I/O, strings, or GUI calls. Parameter automation is sampled per host block; diagnostics use lock-free atomics at 15 UI updates/sec.
- No polyphony, MIDI, graphical editing, harmonies, vibrato controls or production artifact suppression.

Read [architecture](docs/ARCHITECTURE.md) and [validation](docs/VALIDATION.md) before extending the prototype.

## Source repository

- `Source/`: JUCE processor/editor and independent DSP headers.
- `tests/`: DSP, processor/state/editor, and optional VST3 binary-host tests.
- `examples/`: REAPER smoke script and small, intentional reference audio files.
- `docs/`: architecture, validation evidence, UI screenshot and JUCE license notice. The historical test report uses `<BUILD_DIR>` in place of machine-specific paths.
- `CMakeLists.txt`: reproducible build configuration with a pinned JUCE revision.
- `.gitignore` / `.gitattributes`: generated-file exclusions and consistent text/binary handling.

Build outputs, IDE caches, user presets and local environment files stay out of Git. Shareable configuration such as `CMakePresets.json` remains eligible for tracking. Keep prebuilt plugin distributions in release assets rather than source history.

## Dependencies

JUCE retains its own licensing terms; see the [pinned JUCE repository](https://github.com/juce-framework/JUCE/tree/29396c22c93392d6738e021b83196283d6e4d850) and its LICENSE.md. Build integration follows the [JUCE CMake API](https://github.com/juce-framework/JUCE/blob/29396c22c93392d6738e021b83196283d6e4d850/docs/CMake%20API.md). The project source does not vendor JUCE or a compiler.
