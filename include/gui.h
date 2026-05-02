#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

static constexpr int GUI_W = 560;
static constexpr int GUI_H = 240;

// Header bar
static constexpr int HDR_H = 24;

// OLED display panel (left)
static constexpr int DISP_X = 8;
static constexpr int DISP_Y = 26;
static constexpr int DISP_W = 185;
static constexpr int DISP_H = 208;

// Arc needle (within OLED)
static constexpr int ARC_PCX = DISP_X + DISP_W / 2;   // = 100
static constexpr int ARC_PCY = DISP_Y + DISP_H - 66;  // = 168
static constexpr int ARC_R   = 70;

// Piano keyboard (within OLED)
static constexpr int PIANO_KX       = DISP_X + 4;   // = 12
static constexpr int PIANO_KY       = DISP_Y + 8;   // = 34
static constexpr int PIANO_WK_W     = 24;
static constexpr int PIANO_WK_H     = 20;
static constexpr int PIANO_BK_W     = 14;
static constexpr int PIANO_BK_H     = 12;
static constexpr int PIANO_BK_OFFSET = 17; // px from white key left edge to black key

// Right control panel
static constexpr int CTRL_X = DISP_X + DISP_W + 8;  // = 201
static constexpr int CTRL_W = GUI_W - 8 - CTRL_X;   // = 351

// KEY stepper (left section of controls)
static constexpr int KEY_LABEL_X  = 205;
static constexpr int KEY_LABEL_Y  = 34;
static constexpr int KEY_BTN_Y    = 64;   // below 34 + 24(scale=3 text) + 6
static constexpr int KEY_LEFT_X   = 205;
static constexpr int KEY_TEXT_X   = 227;   // KEY_LEFT_X + 16(arrow) + 6(gap)
static constexpr int KEY_RIGHT_X  = 275;   // KEY_TEXT_X + 42(max "A#" @scale=3) + 6

// SCALE stepper (right section of controls)
static constexpr int SCALE_LABEL_X = 317;
static constexpr int SCALE_LABEL_Y = 34;
static constexpr int SCALE_BTN_Y   = 64;
static constexpr int SCALE_LEFT_X  = 317;
static constexpr int SCALE_TEXT_X  = 339;   // SCALE_LEFT_X + 16 + 6
static constexpr int SCALE_RIGHT_X = 534;   // SCALE_TEXT_X + "Chromatic"(189px @scale=3) + 6

// Stepper button size
static constexpr int STEPPER_BTN_W = 16;
static constexpr int STEPPER_BTN_H = 16;

// WIDE slider
static constexpr int WIDE_LABEL_X  = 205;
static constexpr int WIDE_LABEL_Y  = 96;
static constexpr int WIDE_PCT_X    = GUI_W - 8;   // = 552
static constexpr int WIDE_PCT_Y    = 96;
static constexpr int WIDE_TRACK_X  = 205;
static constexpr int WIDE_TRACK_Y  = 124;   // label bottom (96+24) + 4
static constexpr int WIDE_TRACK_W  = GUI_W - 8 - 205;  // = 347
static constexpr int WIDE_TRACK_H  = 3;
static constexpr int SLIDER_THUMB_R = 7;

// TUNE slider
static constexpr int TUNE_LABEL_X  = 205;
static constexpr int TUNE_LABEL_Y  = 164;
static constexpr int TUNE_PCT_X    = GUI_W - 8;   // = 552
static constexpr int TUNE_PCT_Y    = 164;
static constexpr int TUNE_TRACK_X  = 205;
static constexpr int TUNE_TRACK_Y  = 192;   // label bottom (164+24) + 4
static constexpr int TUNE_TRACK_W  = GUI_W - 8 - 205;  // = 347
static constexpr int TUNE_TRACK_H  = 3;

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------

struct GuiColor {
    uint8_t r, g, b;
};

static constexpr GuiColor COL_BG         = { 0x0C, 0x0C, 0x0C };
static constexpr GuiColor COL_DISPLAY_BG = { 0x00, 0x00, 0x00 };
static constexpr GuiColor COL_ACCENT     = { 0x00, 0xFF, 0x88 };
static constexpr GuiColor COL_DIM        = { 0x00, 0x33, 0x22 };
static constexpr GuiColor COL_LABEL      = { 0x55, 0x55, 0x55 };
static constexpr GuiColor COL_TRACK      = { 0x1A, 0x1A, 0x1A };
static constexpr GuiColor COL_FILL       = { 0x00, 0xCC, 0x55 };
static constexpr GuiColor COL_THUMB      = { 0x00, 0xFF, 0x88 };
static constexpr GuiColor COL_WHITE      = { 0xFF, 0xFF, 0xFF };
static constexpr GuiColor COL_HDR_SEP    = { 0x1A, 0x1A, 0x1A };

// ---------------------------------------------------------------------------
// Note / scale name helpers
// ---------------------------------------------------------------------------

static const char *const NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

// Scale order in GUI: 0=Chromatic, 1=Major, 2=Minor
static const char *const SCALE_NAMES_GUI[3] = {
    "Chromatic", "Major", "Minor"
};

// Map GUI scale index -> plugin ScaleType value stored in param_scale
// GUI: 0=Chromatic -> plugin SCALE_CHROMATIC=2
// GUI: 1=Major     -> plugin SCALE_MAJOR=0
// GUI: 2=Minor     -> plugin SCALE_MINOR=1
static constexpr int GUI_SCALE_TO_PARAM[3] = { 2, 0, 1 };
static constexpr int PARAM_TO_GUI_SCALE[3] = { 1, 2, 0 }; // Major->1, Minor->2, Chromatic->0

static inline const char *note_str(int midi_note) {
    if (midi_note < 0) return "--";
    return NOTE_NAMES[midi_note % 12];
}

// ---------------------------------------------------------------------------
// Slider helpers
// ---------------------------------------------------------------------------

// Convert slider value [0,1] to pixel x position within track
static inline int slider_px(float val, int track_x, int track_w) {
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    return track_x + (int)(val * (float)(track_w));
}

// Convert pixel x within track to slider value [0,1]
static inline float slider_val(int px, int track_x, int track_w) {
    float v = (float)(px - track_x) / (float)(track_w);
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

// ---------------------------------------------------------------------------
// Hit test helpers
// ---------------------------------------------------------------------------

static inline bool hit_rect(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static inline bool hit_key_left(int mx, int my) {
    return hit_rect(mx, my, KEY_LEFT_X, KEY_BTN_Y, STEPPER_BTN_W, STEPPER_BTN_H);
}
static inline bool hit_key_right(int mx, int my) {
    return hit_rect(mx, my, KEY_RIGHT_X, KEY_BTN_Y, STEPPER_BTN_W, STEPPER_BTN_H);
}
static inline bool hit_scale_left(int mx, int my) {
    return hit_rect(mx, my, SCALE_LEFT_X, SCALE_BTN_Y, STEPPER_BTN_W, STEPPER_BTN_H);
}
static inline bool hit_scale_right(int mx, int my) {
    return hit_rect(mx, my, SCALE_RIGHT_X, SCALE_BTN_Y, STEPPER_BTN_W, STEPPER_BTN_H);
}

static inline bool hit_wide_track(int mx, int my) {
    return hit_rect(mx, my,
        WIDE_TRACK_X - SLIDER_THUMB_R, WIDE_TRACK_Y - SLIDER_THUMB_R,
        WIDE_TRACK_W + SLIDER_THUMB_R * 2, WIDE_TRACK_H + SLIDER_THUMB_R * 2);
}
static inline bool hit_tune_track(int mx, int my) {
    return hit_rect(mx, my,
        TUNE_TRACK_X - SLIDER_THUMB_R, TUNE_TRACK_Y - SLIDER_THUMB_R,
        TUNE_TRACK_W + SLIDER_THUMB_R * 2, TUNE_TRACK_H + SLIDER_THUMB_R * 2);
}

// Returns semitone 0-11 if the click lands on a piano key, -1 otherwise.
// Black keys take priority (they visually sit on top of white keys).
static inline int hit_piano_key(int mx, int my) {
    if (!hit_rect(mx, my, PIANO_KX, PIANO_KY, PIANO_WK_W * 7, PIANO_WK_H + 1))
        return -1;
    struct BkDef { int note, dx; };
    static const BkDef BK[5] = {
        {1, 17}, {3, 41}, {6, 89}, {8, 113}, {10, 137}
    };
    static const int WK[7] = { 0, 2, 4, 5, 7, 9, 11 };
    // Black keys only occupy the top portion
    if (my < PIANO_KY + PIANO_BK_H) {
        for (int i = 0; i < 5; ++i) {
            if (hit_rect(mx, my, PIANO_KX + BK[i].dx, PIANO_KY, PIANO_BK_W, PIANO_BK_H))
                return BK[i].note;
        }
    }
    int ki = (mx - PIANO_KX) / PIANO_WK_W;
    return (ki >= 0 && ki < 7) ? WK[ki] : -1;
}

// ---------------------------------------------------------------------------
// GUI state struct
// ---------------------------------------------------------------------------

struct GuiState {
    bool  created    = false;
    void *handle     = nullptr;   // platform window handle
    double dpi_scale = 1.0;

    // Drag state
    bool  drag_wide  = false;
    bool  drag_tune  = false;
    int   drag_x0    = 0;
    float drag_v0    = 0.0f;

    // Needle animation state (GUI thread only)
    float    disp_cents     = 0.0f;
    uint32_t last_det_frame = 0;
    int      snap_cooldown  = 0;   // frames to skip before next snap
};

// ---------------------------------------------------------------------------
// Platform-independent GUI API (implemented per platform)
// ---------------------------------------------------------------------------

struct SilvertunePlugin; // forward decl

void gui_create(SilvertunePlugin *p);
void gui_destroy(SilvertunePlugin *p);
void gui_set_scale(SilvertunePlugin *p, double scale);
bool gui_set_parent(SilvertunePlugin *p, void *native_parent);
void gui_show(SilvertunePlugin *p);
void gui_hide(SilvertunePlugin *p);
