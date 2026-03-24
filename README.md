# Silvertune

Hard pitch correction CLAP plugin for Linux — the "Cher effect."

Snaps incoming audio pitch to the nearest note in a user-selected key/scale with no smoothing, preserving phase vocoder artifacts for the characteristic hard autotune sound.

## Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| Key | C through B | Root note of the scale |
| Scale | Major / Minor / Chromatic | Scale to quantize to |
| Mix | 0–100% | Dry/wet blend |
| Speed | 0–100% | 0% = instant hard snap (Cher), 100% = gentler correction |

## Dependencies

```bash
sudo pacman -S rubberband aubio cmake
```

## Build

```bash
git clone --recurse-submodules https://github.com/yourusername/silvertune
cd silvertune
mkdir build && cd build
cmake ..
make
cp silvertune.clap ~/.clap/
```

## Usage

Load `Silvertune` as an audio effect in Bitwig Studio (or any CLAP host). Set your key and scale, turn mix to 100%, speed to 0% for maximum Cher.

## Technical Details

- **Pitch detection**: aubio YIN algorithm, confidence threshold 0.8
- **Pitch shifting**: Rubber Band Library in real-time mode with crisp transients
- **Latency**: Reported to host for automatic compensation
- **Audio**: Stereo in/out, 44.1/48kHz, buffer sizes 64–1024
