# Roadmap

## v0.1.0 — now
CLAP plugin. Linux. Four knobs. Zero latency. Sounds like Waves Tune at 0ms/0ms.

## v0.2.0 — the pedal
Port the DSP to a Daisy Seed or Teensy 4.1. Strip aubio, write bare YIN in ~50 lines of C. Four pots: Key, Scale, Mix, Snap. One stomp switch: foot on = robot, foot off = raw. Mid-song. Mid-word. The first autotune pedal for punk.

## v0.3.0 — character
A fifth knob that controls the grain size and correction behavior. Small grains = more digital, more robotic. Hard note locking so every scale degree is heard as a discrete stair step. The Cher knob.

## v0.4.0 — pitch detection hardening
Switch to yinfft with spectral weighting for better octave accuracy on belted vocals and open vowels. Octave-jump rejection filter. Lower confidence threshold so correction doesn't bail out mid-phrase.

## v0.5.0 — tuner display
Minimal UI via Pugl. A single arc showing detected pitch vs. snapped pitch. Nothing else. No skeuomorphic knobs, no fake chrome, no waveform displays.

## someday, maybe
- Formant preservation toggle
- MIDI input to override key from DAW
- macOS / Windows builds
- Ardour preset integration
- Duo mode: two singers, two keys, split by frequency range
