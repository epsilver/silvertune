#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

static constexpr int GUI_W = 640;
static constexpr int GUI_H = 300;

// Header bar
static constexpr int HDR_H = 24;

// OLED display panel (left)
static constexpr int DISP_X = 8;
static constexpr int DISP_Y = 28;
static constexpr int DISP_W = 200;
static constexpr int DISP_H = 264;

// Arc needle (within OLED)
static constexpr int ARC_PCX = DISP_X + DISP_W / 2;   // = 108
static constexpr int ARC_PCY = DISP_Y + DISP_H - 80;  // = 212
static constexpr int ARC_R   = 80;

// Piano keyboard (within OLED)
static constexpr int PIANO_KX        = DISP_X + 4;   // = 12
static constexpr int PIANO_KY        = DISP_Y + 8;   // = 36
static constexpr int PIANO_WK_W      = 26;
static constexpr int PIANO_WK_H      = 22;
static constexpr int PIANO_BK_W      = 16;
static constexpr int PIANO_BK_H      = 14;
static constexpr int PIANO_BK_OFFSET = 19; // px from white key left edge to black key

// Right control panel
static constexpr int CTRL_X = DISP_X + DISP_W + 8;   // = 216

// KEY stepper
// Positions sized for Inter-Black 14px: "A#" ~20px, "Chromatic" ~70px
static constexpr int KEY_LABEL_X  = 224;
static constexpr int KEY_LABEL_Y  = 36;
static constexpr int KEY_BTN_Y    = 58;   // label_y + ~14px text + 8 gap
static constexpr int KEY_LEFT_X   = 224;
static constexpr int KEY_TEXT_X   = 246;   // KEY_LEFT_X + 16(arrow) + 6(gap)
static constexpr int KEY_RIGHT_X  = 290;   // KEY_TEXT_X + ~30px (A# + margin) + 14

// SCALE stepper
static constexpr int SCALE_LABEL_X = 380;
static constexpr int SCALE_LABEL_Y = 36;
static constexpr int SCALE_BTN_Y   = 58;
static constexpr int SCALE_LEFT_X  = 380;
static constexpr int SCALE_TEXT_X  = 402;   // SCALE_LEFT_X + 16 + 6
static constexpr int SCALE_RIGHT_X = 550;   // SCALE_TEXT_X + ~132px ("Chromatic" + margin) + 16

// Stepper button size
static constexpr int STEPPER_BTN_W = 16;
static constexpr int STEPPER_BTN_H = 16;

// WIDE slider
static constexpr int WIDE_LABEL_X  = 224;
static constexpr int WIDE_LABEL_Y  = 106;
static constexpr int WIDE_PCT_X    = GUI_W - 8;   // = 632
static constexpr int WIDE_PCT_Y    = 106;
static constexpr int WIDE_TRACK_X  = 224;
static constexpr int WIDE_TRACK_Y  = 126;
static constexpr int WIDE_TRACK_W  = GUI_W - 8 - 224;  // = 408
static constexpr int WIDE_TRACK_H  = 3;
static constexpr int SLIDER_THUMB_R = 8;

// TUNE slider
static constexpr int TUNE_LABEL_X  = 224;
static constexpr int TUNE_LABEL_Y  = 204;
static constexpr int TUNE_PCT_X    = GUI_W - 8;   // = 632
static constexpr int TUNE_PCT_Y    = 204;
static constexpr int TUNE_TRACK_X  = 224;
static constexpr int TUNE_TRACK_Y  = 224;
static constexpr int TUNE_TRACK_W  = GUI_W - 8 - 224;  // = 408
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

static const char *const SCALE_NAMES_GUI[3] = {
    "Chromatic", "Major", "Minor"
};

static constexpr int GUI_SCALE_TO_PARAM[3] = { 2, 0, 1 };
static constexpr int PARAM_TO_GUI_SCALE[3] = { 1, 2, 0 };

static inline const char *note_str(int midi_note) {
    if (midi_note < 0) return "--";
    return NOTE_NAMES[midi_note % 12];
}

// ---------------------------------------------------------------------------
// Slider helpers
// ---------------------------------------------------------------------------

static inline int slider_px(float val, int track_x, int track_w) {
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    return track_x + (int)(val * (float)(track_w));
}

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
static inline int hit_piano_key(int mx, int my) {
    if (!hit_rect(mx, my, PIANO_KX, PIANO_KY, PIANO_WK_W * 7, PIANO_WK_H + 1))
        return -1;
    struct BkDef { int note, dx; };
    static const BkDef BK[5] = {
        {1, 19}, {3, 45}, {6, 97}, {8, 123}, {10, 149}
    };
    static const int WK[7] = { 0, 2, 4, 5, 7, 9, 11 };
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
    void *handle     = nullptr;
    double dpi_scale = 1.0;

    // Drag state
    bool  drag_wide  = false;
    bool  drag_tune  = false;
    int   drag_x0    = 0;
    float drag_v0    = 0.0f;

    // Needle animation state (GUI thread only)
    float    disp_cents     = 0.0f;
    uint32_t last_det_frame = 0;
    int      snap_cooldown  = 0;
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
