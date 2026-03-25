# Roadmap

## v0.1.0 — now
CLAP plugin. Linux. Four knobs. Zero latency. Sounds like Waves Tune at 0ms/0ms.

## v0.2.0 — the pedal
Port the DSP to a Daisy Seed. Strip aubio, write bare YIN in ~50 lines of C. Six knobs, one stomp switch: foot on = robot, foot off = raw. Mid-song. Mid-word. The first autotune pedal for punk.

**The build:**
- Terrarium PCB from PedalPCB (~$10)
- 125B aluminum enclosure from Tayda (~$5, custom drilled with Terrarium template)
- Daisy Seed from Electrosmith (~$30)
- 6 pots, mono jacks, DC jack, footswitch, LED, pin headers — all through-hole
- Total: ~$60-70. Soldering iron and patience. No SMD. DIY af.

Separate repo. Same DSP brain.

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
