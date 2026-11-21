# FlatTop

A ZoidClip-style soft clipper VST3 for FL Studio (Windows / macOS). Built on the same stack as
[PeakEater](https://github.com/vvvar/PeakEater) (JUCE + CMake). It is a fresh project, not a patch of
PeakEater's source.

## Signal flow

```
INPUT -> SOFT CLIP (threshold, makeup built in; SINGLE or MULTI 3-band) -> POST
      -> 3-BAND MASTER EQ -> SAT -> SHEEN (+3.9 kHz image) -> DENSITY (drive)
      -> HARD CLIP @ 0 dBFS (not oversampled) -> OUTPUT
```

Because the EQ and SAT sit *before* the ceiling, every boost you draw gets clipped too.

## Controls

| Control | What it does |
|---|---|
| **THRESHOLD** (big knob) | Soft-clip threshold, -24..0 dB. Makeup is built in: the signal is scaled by 1/threshold before a smooth tanh knee that settles at exactly 0 dBFS. Lower threshold = peaks fold over smoothly *and the mix comes up* instead of just flattening. |
| **ON** button (above threshold) | Lit green = threshold stage active. Dark/unlit = soft-clip stage (threshold + makeup) bypassed completely. |
| **INPUT / POST / OUTPUT** | Input = gain into the clipper. Post = level right after the soft-clip stage. Output = final trim after the ceiling (max 0 dB, so the ceiling is never exceeded). |
| **SAT** | tanh saturation between the clipper/EQ and the ceiling. |
| **SHEEN** | Takes the highs (>3 kHz), shifts them **up by 3.9 kHz** (single-sideband) and adds them back. The shifted image is deliberately not band-limited, so anything pushed past Nyquist folds back as aliasing, like a low-quality FL export. Most audible at 44.1/48 kHz. |
| **DENSITY** | 0..+12 dB of drive straight into the 0 dBFS hard ceiling: peaks flat-top, drums hit harder, the mix packs in. |
| **MULTI** | Splits the clipper into lows / mids / highs (24 dB/oct Linkwitz-Riley, phase-aligned). Each band gets its own **DRIVE** (0..+24 dB) into the threshold, so you can smash the 808 and leave the hats alone, or the reverse. Crossovers: **LOW / MID** and **MID / HIGH** knobs. Greyed out while MULTI is off. |
| **3-band master EQ** | Three vertical faders (low shelf / mid bell / high shelf, +-12 dB). **Double-click a fader to zero it.** Small, equal-size FREQ knobs under each fader set the band frequency (double-click resets those too). |

Knob sizes: THRESHOLD is the largest, all other knobs are identical, EQ knobs are smaller and identical.

## Get the .vst3 (no compiler needed): GitHub Actions

1. Create a new GitHub repo and upload everything in this folder (drag-and-drop works in the browser,
   keep the `.github` folder).
2. Open the **Actions** tab. The "Build FlatTop VST3" workflow runs on every push (about 10 minutes).
3. Open the finished run and download **FlatTop-VST3-Windows** (or **-macOS**) from *Artifacts*.

## Build locally

Windows: install Visual Studio 2022 Build Tools (C++ workload) and CMake, then:

```
cmake -B build
cmake --build build --config Release --target FlatTop_VST3
```

The plugin lands in `build/FlatTop_artefacts/Release/VST3/FlatTop.vst3`.
macOS needs Xcode command line tools + CMake (same commands). The first configure downloads JUCE.

Optional DSP unit tests (no JUCE needed):

```
g++ -std=c++17 -O2 -Isource tests/dsp_test.cpp -o dsp_test && ./dsp_test
```

## Install in FL Studio

* **Windows:** copy the whole `FlatTop.vst3` folder to `C:\Program Files\Common Files\VST3\`
* **macOS:** copy it to `/Library/Audio/Plug-ins/VST3/`. Unsigned builds may need
  `xattr -cr /Library/Audio/Plug-ins/VST3/FlatTop.vst3` once.
* In FL Studio: *Options > Manage plugins > Find installed plugins* (verify the VST3 folder is in the scan list), then add FlatTop from the plugin database.

## Notes

* Hard clip and sheen are intentionally **not oversampled**; expect aliasing at high drive. That is the sound.
* The DSP core (`source/FlatTopDSP.h`) has no JUCE dependency and is covered by `tests/dsp_test.cpp`
  (flat crossover sum, makeup gain, EQ response, 0 dBFS ceiling, sheen image position and fold-back).
* JUCE is dual-licensed (AGPLv3 / commercial); PeakEater is GPL-3.0. Personal use is fine; if you
  distribute the plugin, publish the source under a compatible license.
