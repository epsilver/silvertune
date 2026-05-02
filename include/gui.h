#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

static constexpr int GUI_W = 480;
static constexpr int GUI_H = 200;

// Left display panel
static constexpr int DISP_X = 8;
static constexpr int DISP_Y = 8;
static constexpr int DISP_W = 148;
static constexpr int DISP_H = 184;

// KEY stepper
static constexpr int KEY_LABEL_X  = 168;
static constexpr int KEY_LABEL_Y  = 24;
static constexpr int KEY_BTN_Y    = 38;
static constexpr int KEY_LEFT_X   = 168;
static constexpr int KEY_TEXT_X   = 190;
static constexpr int KEY_RIGHT_X  = 232;

// SCALE stepper
static constexpr int SCALE_LABEL_X = 296;
static constexpr int SCALE_LABEL_Y = 24;
static constexpr int SCALE_BTN_Y   = 38;
static constexpr int SCALE_LEFT_X  = 296;
static constexpr int SCALE_TEXT_X  = 318;
static constexpr int SCALE_RIGHT_X = 380;

// WIDE slider
static constexpr int WIDE_LABEL_X  = 168;
static constexpr int WIDE_LABEL_Y  = 92;
static constexpr int WIDE_PCT_X    = 440;
static constexpr int WIDE_PCT_Y    = 92;
static constexpr int WIDE_TRACK_X  = 168;
static constexpr int WIDE_TRACK_Y  = 108;
static constexpr int WIDE_TRACK_W  = 300;
static constexpr int WIDE_TRACK_H  = 8;
static constexpr int SLIDER_THUMB_R = 7;

// TUNE slider
static constexpr int TUNE_LABEL_X  = 168;
static constexpr int TUNE_LABEL_Y  = 144;
static constexpr int TUNE_PCT_X    = 440;
static constexpr int TUNE_PCT_Y    = 144;
static constexpr int TUNE_TRACK_X  = 168;
static constexpr int TUNE_TRACK_Y  = 160;
static constexpr int TUNE_TRACK_W  = 300;
static constexpr int TUNE_TRACK_H  = 8;

// Stepper button size
static constexpr int STEPPER_BTN_W = 16;
static constexpr int STEPPER_BTN_H = 16;

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------

struct GuiColor {
    uint8_t r, g, b;
};

static constexpr GuiColor COL_BG           = { 0x11, 0x11, 0x11 };
static constexpr GuiColor COL_DISPLAY_BG   = { 0x00, 0x00, 0x00 };
static constexpr GuiColor COL_GREEN        = { 0x00, 0xE0, 0x66 };
static constexpr GuiColor COL_DIM_GREEN    = { 0x00, 0x55, 0x22 };
static constexpr GuiColor COL_LABEL        = { 0x88, 0x88, 0x88 };
static constexpr GuiColor COL_TRACK        = { 0x2A, 0x2A, 0x2A };
static constexpr GuiColor COL_FILL         = { 0x00, 0xAA, 0x44 };
static constexpr GuiColor COL_THUMB        = { 0x00, 0xFF, 0x66 };
static constexpr GuiColor COL_WHITE        = { 0xFF, 0xFF, 0xFF };

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
