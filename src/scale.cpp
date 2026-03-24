#include "silvertune.h"
#include <cmath>

// Major scale intervals: W W H W W W H
static const bool major_notes[12] = {
    true,  false, true,  false, true,  true,
    false, true,  false, true,  false, true
};

// Natural minor scale intervals: W H W W H W W
static const bool minor_notes[12] = {
    true,  false, true,  true,  false, true,
    false, true,  true,  false, true,  false
};

int quantize_to_scale(int midi_note, int root_key, ScaleType scale) {
    if (scale == SCALE_CHROMATIC) {
        return midi_note;
    }

    const bool *pattern = (scale == SCALE_MAJOR) ? major_notes : minor_notes;

    int note_class = ((midi_note % 12) - root_key + 12) % 12;
    int octave_base = midi_note - note_class;

    if (pattern[note_class])
        return midi_note;

    // Search outward for nearest scale note, round up on ties
    for (int offset = 1; offset <= 6; ++offset) {
        int up = (note_class + offset) % 12;
        int down = (note_class - offset + 12) % 12;

        bool up_in = pattern[up];
        bool down_in = pattern[down];

        if (up_in && down_in) {
            // Tie: round up
            return octave_base + note_class + offset;
        }
        if (up_in)
            return octave_base + note_class + offset;
        if (down_in)
            return octave_base + note_class - offset;
    }

    return midi_note;
}

float midi_to_hz(float midi) {
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

float hz_to_midi(float hz) {
    if (hz <= 0.0f) return 0.0f;
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}
