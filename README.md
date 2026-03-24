# Silvertune

```
    A4 ──── 440.00 Hz ─── ♪ SnapSnap ──── A4
    G#4 ─── 415.30 Hz ─── ♪ SnapSnap ──── A4
    Bb4 ─── 466.16 Hz ─── ♪ SnapSnap ──── A4
```

**Hard pitch correction for Linux.** A real-time CLAP audio plugin that quantizes every note you sing to the nearest tone in your chosen key — instantly, mercilessly, beautifully. The Cher effect. The T-Pain shimmer. The robot in the mirror singing back at you in perfect tune.

Your voice enters at 417 Hz. Silvertune hears G#4, knows you meant A4, and snaps you to 440 Hz before the next sample hits the buffer. No smoothing. No negotiation. Every frequency bent to the nearest degree of the scale like light through a prism.

## How it sounds

Set the key to **G Major** and sing a melody. Every note locks to one of seven frequencies:

```
G3  ── 196.00 Hz
A3  ── 220.00 Hz
B3  ── 246.94 Hz
C4  ── 261.63 Hz
D4  ── 293.66 Hz
E4  ── 329.63 Hz
F#4 ── 369.99 Hz
G4  ── 392.00 Hz
```

Nothing in between. Your vibrato disappears. Your pitch bends flatten. What remains is a voice made of glass — crystalline, inhuman, strangely perfect.

## Parameters

| Knob | Range | What it does |
|------|-------|--------------|
| **Key** | C C# D D# E F F# G G# A A# B | Root note — the tonal center everything snaps to |
| **Scale** | Major / Minor / Chromatic | Which seven notes (or all twelve) are legal |
| **Mix** | 0 – 100% | Dry voice vs. corrected voice |
| **Speed** | 0 – 100% | 0% = hard snap. 100% = gentle nudge. You want 0%. |

## Architecture

```
  audio in ─┬─► aubio YIN pitch detector ─► 417 Hz detected
             │                                    │
             │                              quantize to scale
             │                                    │
             │                               A4 = 440 Hz
             │                                    │
             │                          ratio = 440 / 417 = 1.055
             │                                    │
             └──► grain pitch shifter ◄───── shift by 1.055x
                         │
                    mix dry/wet
                         │
                    audio out ─► 440 Hz ♪
```

Two crossfaded read heads sweep through a circular delay line. The ratio between read speed and write speed shifts the pitch. A Hann window at each grain boundary keeps the splice inaudible — or deliberately audible, depending on how hard you push it.

**Zero latency.** No FFT look-ahead, no phase vocoder buffering. Sample-in, sample-out. Your DAW's buffer size is the only delay.

## Build

```bash
sudo pacman -S aubio cmake
git clone --recurse-submodules https://github.com/epsilver/silvertune
cd silvertune
mkdir build && cd build
cmake ..
make
cp silvertune.clap ~/.clap/
```

## Stack

| Layer | What | Why |
|-------|------|-----|
| Format | CLAP | Modern, open, Bitwig-native |
| Pitch detection | aubio (YIN algorithm) | Fast, accurate monophonic tracking |
| Pitch shifting | Custom grain shifter | Zero latency, artifact character |
| Audio | Stereo in/out, 44.1–48 kHz | Standard studio rates |
| Platform | Linux (Arch / CachyOS) | Because that's where we live |
| Language | C++17 | Because that's what plugins speak |

## License

Do whatever you want with it. Sing in tune. Sing out of tune. Let the machine decide.
