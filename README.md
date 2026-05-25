# Silvertune

Hard pitch correction plugin. Hand-rolled YIN detector, 2-tap grain shifter, no external audio dependencies.

CLAP · VST3 · AUv2 — Linux · macOS · Windows

---

## Parameters

| Parameter | Range | What it does |
|---|---|---|
| **Key** | C – B | Root note |
| **Scale** | Major / Minor / Chromatic | Scale to snap to; Chromatic disables correction |
| **Speed** | 0 – 100 ms | Exponential glide time to the target pitch (0 = instant snap) |
| **Hold** | 0 – 200 ms | How long a note must be stable before correction commits |
| **Wide** | 0 – 100% | Stereo doubler blend (slightly detuned second grain) |

---

## Architecture

```
audio in
    │
    ├──► YIN pitch detector  (BUF=1024, HOP=128)
    │         │
    │    stays_locked hysteresis (±0.4 st)
    │         │
    │    hold timer → quantize_to_scale → pitch ratio
    │         │
    │    exponential chase  (Speed parameter)
    │         │
    ├──► grain shifter A  ──────────────────┐
    ├──► grain shifter B  (×DETUNE) × Wide ─┤
    │                                        │
    │                                   sum + noise gate
    │                                        │
    └────────────────────────────────── audio out
```

### Pitch detection

YIN difference function on a 1024-sample circular buffer, evaluated every 128 samples. Parabolic interpolation refines the period estimate. A `stays_locked` guard (±0.4 semitones) prevents robo-vibrato on sustained notes while still tracking note changes. The Hold timer delays committing a new correction ratio until the detected pitch is stable.

### Grain shifter

Two overlapping grains at phase offsets 0 and 0.5 of a 256-sample window, each with a Hann envelope. The 0.5-offset pair sums to unity gain (COLA). A second instance feeds the Wide doubler at a fixed detune ratio.

### Noise gate

Leaky-integrator RMS gate with a fast attack (~0.5 ms) and slow release (~10 ms) prevents the grain shifter from processing silence and leaking artefacts.

---

## Build

```bash
git clone --recurse-submodules https://github.com/epsilver/silvertune
cd silvertune
cmake -B build
cmake --build build
```

Install:

```bash
# Linux
cp build/Silvertune.clap ~/.clap/
cp -r build/Release/Silvertune.vst3 ~/.vst3/

# macOS
cp build/Silvertune.clap ~/Library/Audio/Plug-Ins/CLAP/
cp -r build/Release/Silvertune.vst3 ~/Library/Audio/Plug-Ins/VST3/

# macOS — if your DAW won't scan it
xattr -rd com.apple.quarantine Silvertune.clap

# Windows
copy build\Release\Silvertune.clap %APPDATA%\CLAP\
```

---

## Stack

| | |
|---|---|
| Plugin format | CLAP (native) · VST3 · AUv2 via clap-wrapper |
| Pitch detection | Hand-rolled YIN (no aubio) |
| Pitch shifting | Hand-rolled 2-tap grain shifter |
| GUI | Native: X11 (Linux) · Win32 (Windows) · Cocoa (macOS) |
| Language | C++20 |
