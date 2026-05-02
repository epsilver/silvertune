#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gui.h"
#include "silvertune.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Window class registration
// ---------------------------------------------------------------------------

static const char *WNDCLASS_NAME = "SilvertuneGUI";

static bool register_wnd_class() {
    static bool registered = false;
    if (registered) return true;

    WNDCLASSEXA wc = {};
    if (GetClassInfoExA(GetModuleHandleA(nullptr), WNDCLASS_NAME, &wc)) {
        registered = true;
        return true;
    }

    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = DefWindowProcA; // replaced per-window via SetWindowLongPtr
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = WNDCLASS_NAME;
    wc.cbWndExtra    = sizeof(void *); // store SilvertunePlugin*

    if (!RegisterClassExA(&wc)) return false;
    registered = true;
    return true;
}

// ---------------------------------------------------------------------------
// Fonts (created once per process, stored as statics)
// ---------------------------------------------------------------------------

static HFONT s_font_label = nullptr;
static HFONT s_font_value = nullptr;
static HFONT s_font_note  = nullptr;

static void ensure_fonts() {
    if (!s_font_label)
        s_font_label = CreateFontA(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
    if (!s_font_value)
        s_font_value = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
    if (!s_font_note)
        s_font_note = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void fill_rect_dc(HDC dc, int x, int y, int w, int h, COLORREF col) {
    RECT r = { x, y, x + w, y + h };
    HBRUSH br = CreateSolidBrush(col);
    FillRect(dc, &r, br);
    DeleteObject(br);
}

static void draw_rect_dc(HDC dc, int x, int y, int w, int h, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HPEN old = (HPEN)SelectObject(dc, pen);
    HBRUSH old_br = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, x, y, x + w, y + h);
    SelectObject(dc, old);
    SelectObject(dc, old_br);
    DeleteObject(pen);
}

static void draw_text_dc(HDC dc, int x, int y, const char *text, HFONT font, COLORREF col,
                          bool right_align = false) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SetTextColor(dc, col);
    SetBkMode(dc, TRANSPARENT);
    if (right_align) {
        SIZE sz = {};
        GetTextExtentPoint32A(dc, text, (int)strlen(text), &sz);
        x -= sz.cx;
    }
    TextOutA(dc, x, y, text, (int)strlen(text));
    SelectObject(dc, old);
}

static void draw_left_triangle_dc(HDC dc, int bx, int by, COLORREF col) {
    POINT pts[3] = {
        { bx + STEPPER_BTN_W - 3, by + 3 },
        { bx + 3,                  by + STEPPER_BTN_H / 2 },
        { bx + STEPPER_BTN_W - 3, by + STEPPER_BTN_H - 3 },
    };
    HBRUSH br = CreateSolidBrush(col);
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HBRUSH old_br = (HBRUSH)SelectObject(dc, br);
    HPEN   old_pen = (HPEN)SelectObject(dc, pen);
    Polygon(dc, pts, 3);
    SelectObject(dc, old_br);
    SelectObject(dc, old_pen);
    DeleteObject(br);
    DeleteObject(pen);
}

static void draw_right_triangle_dc(HDC dc, int bx, int by, COLORREF col) {
    POINT pts[3] = {
        { bx + 3,                  by + 3 },
        { bx + STEPPER_BTN_W - 3, by + STEPPER_BTN_H / 2 },
        { bx + 3,                  by + STEPPER_BTN_H - 3 },
    };
    HBRUSH br = CreateSolidBrush(col);
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HBRUSH old_br = (HBRUSH)SelectObject(dc, br);
    HPEN   old_pen = (HPEN)SelectObject(dc, pen);
    Polygon(dc, pts, 3);
    SelectObject(dc, old_br);
    SelectObject(dc, old_pen);
    DeleteObject(br);
    DeleteObject(pen);
}

static void fill_circle_dc(HDC dc, int cx, int cy, int r, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HBRUSH old_br = (HBRUSH)SelectObject(dc, br);
    HPEN   old_pen = (HPEN)SelectObject(dc, pen);
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(dc, old_br);
    SelectObject(dc, old_pen);
    DeleteObject(br);
    DeleteObject(pen);
}

// ---------------------------------------------------------------------------
// Full paint routine
// ---------------------------------------------------------------------------

static void do_paint(HWND hwnd, HDC hdc) {
    HWND parent = GetParent(hwnd);
    LONG_PTR lp = GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    SilvertunePlugin *p = reinterpret_cast<SilvertunePlugin *>(lp);
    if (!p) return;

    ensure_fonts();

    // Double buffer
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, GUI_W, GUI_H);
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

    COLORREF col_bg         = RGB(0x11, 0x11, 0x11);
    COLORREF col_display_bg = RGB(0x00, 0x00, 0x00);
    COLORREF col_green      = RGB(0x00, 0xE0, 0x66);
    COLORREF col_dim_green  = RGB(0x00, 0x55, 0x22);
    COLORREF col_label      = RGB(0x88, 0x88, 0x88);
    COLORREF col_track      = RGB(0x2A, 0x2A, 0x2A);
    COLORREF col_fill       = RGB(0x00, 0xAA, 0x44);
    COLORREF col_thumb      = RGB(0x00, 0xFF, 0x66);

    // Background
    fill_rect_dc(mem_dc, 0, 0, GUI_W, GUI_H, col_bg);

    // --- Left display panel ---
    draw_rect_dc(mem_dc, DISP_X, DISP_Y, DISP_W, DISP_H, col_dim_green);
    fill_rect_dc(mem_dc, DISP_X + 1, DISP_Y + 1, DISP_W - 2, DISP_H - 2, col_display_bg);

    // Title
    draw_text_dc(mem_dc, DISP_X + 4, DISP_Y + 4, "SILVERTUNE", s_font_label, col_green);

    // Separator (approximate position — label font ~10pt → ~13px)
    {
        int sy = DISP_Y + 4 + 14;
        HPEN pen = CreatePen(PS_SOLID, 1, col_dim_green);
        HPEN old = (HPEN)SelectObject(mem_dc, pen);
        MoveToEx(mem_dc, DISP_X + 1, sy, nullptr);
        LineTo(mem_dc, DISP_X + DISP_W - 2, sy);
        SelectObject(mem_dc, old);
        DeleteObject(pen);
    }

    // DET label + note
    {
        draw_text_dc(mem_dc, DISP_X + 4, DISP_Y + 24, "DET", s_font_label, col_label);
        int det = p->gui_det.load(std::memory_order_relaxed);
        const char *det_str = (det >= 0) ? NOTE_NAMES[det % 12] : "--";
        draw_text_dc(mem_dc, DISP_X + 4, DISP_Y + 40, det_str, s_font_note, col_green);
    }

    // CORR label + note
    {
        draw_text_dc(mem_dc, DISP_X + 4, DISP_Y + 78, "CORR", s_font_label, col_label);
        int corr = p->gui_corr.load(std::memory_order_relaxed);
        const char *corr_str = (corr >= 0) ? NOTE_NAMES[corr % 12] : "--";
        draw_text_dc(mem_dc, DISP_X + 4, DISP_Y + 94, corr_str, s_font_note, col_green);
    }

    // RMS bar
    {
        float rms = p->gui_rms.load(std::memory_order_relaxed);
        float rms_c = rms > 1.0f ? 1.0f : rms;
        int bar_x = DISP_X + 4;
        int bar_y = DISP_Y + DISP_H - 16;
        int bar_max_w = DISP_W - 8;
        int bar_h = 8;
        fill_rect_dc(mem_dc, bar_x, bar_y, bar_max_w, bar_h, col_track);
        if (rms_c > 0.0f) {
            int fw = (int)(rms_c * bar_max_w);
            if (fw > 0) fill_rect_dc(mem_dc, bar_x, bar_y, fw, bar_h, col_fill);
        }
    }

    // --- KEY stepper ---
    draw_text_dc(mem_dc, KEY_LABEL_X, KEY_LABEL_Y, "KEY", s_font_label, col_label);
    draw_left_triangle_dc(mem_dc, KEY_LEFT_X, KEY_BTN_Y, col_label);
    draw_right_triangle_dc(mem_dc, KEY_RIGHT_X, KEY_BTN_Y, col_label);
    {
        int key = (int)std::lround(p->param_key.load());
        key = ((key % 12) + 12) % 12;
        draw_text_dc(mem_dc, KEY_TEXT_X, KEY_BTN_Y, NOTE_NAMES[key], s_font_value, col_green);
    }

    // --- SCALE stepper ---
    draw_text_dc(mem_dc, SCALE_LABEL_X, SCALE_LABEL_Y, "SCALE", s_font_label, col_label);
    draw_left_triangle_dc(mem_dc, SCALE_LEFT_X, SCALE_BTN_Y, col_label);
    draw_right_triangle_dc(mem_dc, SCALE_RIGHT_X, SCALE_BTN_Y, col_label);
    {
        int ps = (int)std::lround(p->param_scale.load());
        ps = ps < 0 ? 0 : (ps > 2 ? 2 : ps);
        int gs = PARAM_TO_GUI_SCALE[ps];
        draw_text_dc(mem_dc, SCALE_TEXT_X, SCALE_BTN_Y, SCALE_NAMES_GUI[gs], s_font_value, col_green);
    }

    // --- WIDE slider ---
    {
        float wide = (float)p->param_wide.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", wide * 100.0f);
        draw_text_dc(mem_dc, WIDE_LABEL_X, WIDE_LABEL_Y, "WIDE", s_font_label, col_label);
        draw_text_dc(mem_dc, WIDE_PCT_X, WIDE_PCT_Y, pct, s_font_value, col_label, true);
        fill_rect_dc(mem_dc, WIDE_TRACK_X, WIDE_TRACK_Y, WIDE_TRACK_W, WIDE_TRACK_H, col_track);
        int fw = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W) - WIDE_TRACK_X;
        if (fw > 0) fill_rect_dc(mem_dc, WIDE_TRACK_X, WIDE_TRACK_Y, fw, WIDE_TRACK_H, col_fill);
        int tx = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W);
        int ty = WIDE_TRACK_Y + WIDE_TRACK_H / 2;
        fill_circle_dc(mem_dc, tx, ty, SLIDER_THUMB_R, col_thumb);
    }

    // --- TUNE slider ---
    {
        float tune = (float)p->param_speed.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", tune * 100.0f);
        draw_text_dc(mem_dc, TUNE_LABEL_X, TUNE_LABEL_Y, "TUNE", s_font_label, col_label);
        draw_text_dc(mem_dc, TUNE_PCT_X, TUNE_PCT_Y, pct, s_font_value, col_label, true);
        fill_rect_dc(mem_dc, TUNE_TRACK_X, TUNE_TRACK_Y, TUNE_TRACK_W, TUNE_TRACK_H, col_track);
        int fw = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W) - TUNE_TRACK_X;
        if (fw > 0) fill_rect_dc(mem_dc, TUNE_TRACK_X, TUNE_TRACK_Y, fw, TUNE_TRACK_H, col_fill);
        int tx = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W);
        int ty = TUNE_TRACK_Y + TUNE_TRACK_H / 2;
        fill_circle_dc(mem_dc, tx, ty, SLIDER_THUMB_R, col_thumb);
    }

    BitBlt(hdc, 0, 0, GUI_W, GUI_H, mem_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

// ---------------------------------------------------------------------------
// Parameter change helper
// ---------------------------------------------------------------------------

static void set_param_and_notify(SilvertunePlugin *p, int param_id, double value) {
    switch (param_id) {
    case PARAM_KEY:   p->param_key.store(value);   break;
    case PARAM_SCALE: p->param_scale.store(value); break;
    case PARAM_WIDE:  p->param_wide.store(value);  break;
    case PARAM_SPEED: p->param_speed.store(value); break;
    }
    p->host->request_callback(p->host);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK silvertune_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SilvertunePlugin *p = reinterpret_cast<SilvertunePlugin *>(
        GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        do_paint(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        if (!p) return 0;
        int mx = LOWORD(lp);
        int my = HIWORD(lp);
        SetCapture(hwnd);

        if (hit_key_left(mx, my)) {
            int k = (int)std::lround(p->param_key.load());
            k = ((k - 1) % 12 + 12) % 12;
            set_param_and_notify(p, PARAM_KEY, (double)k);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (hit_key_right(mx, my)) {
            int k = (int)std::lround(p->param_key.load());
            k = (k + 1) % 12;
            set_param_and_notify(p, PARAM_KEY, (double)k);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (hit_scale_left(mx, my)) {
            int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
            s = ((s - 1) % 3 + 3) % 3;
            set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (hit_scale_right(mx, my)) {
            int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
            s = (s + 1) % 3;
            set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (hit_wide_track(mx, my)) {
            p->gui.drag_wide = true;
            p->gui.drag_tune = false;
            p->gui.drag_x0   = mx;
            p->gui.drag_v0   = (float)p->param_wide.load();
            float v = slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W);
            set_param_and_notify(p, PARAM_WIDE, (double)v);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (hit_tune_track(mx, my)) {
            p->gui.drag_tune = true;
            p->gui.drag_wide = false;
            p->gui.drag_x0   = mx;
            p->gui.drag_v0   = (float)p->param_speed.load();
            float v = slider_val(mx, TUNE_TRACK_X, TUNE_TRACK_W);
            set_param_and_notify(p, PARAM_SPEED, (double)v);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!p) return 0;
        int mx = LOWORD(lp);
        if (p->gui.drag_wide) {
            float v = slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W);
            set_param_and_notify(p, PARAM_WIDE, (double)v);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (p->gui.drag_tune) {
            float v = slider_val(mx, TUNE_TRACK_X, TUNE_TRACK_W);
            set_param_and_notify(p, PARAM_SPEED, (double)v);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (p) {
            p->gui.drag_wide = false;
            p->gui.drag_tune = false;
        }
        ReleaseCapture();
        return 0;

    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

// ---------------------------------------------------------------------------
// Platform GUI API
// ---------------------------------------------------------------------------

void gui_create(SilvertunePlugin *p) {
    register_wnd_class();
    p->gui.created = true;
}

bool gui_set_parent(SilvertunePlugin *p, void *native_parent) {
    if (!p->gui.created) return false;

    HWND parent_hwnd = static_cast<HWND>(native_parent);

    // Re-register with our WndProc (class was registered with DefWindowProc)
    // Use per-instance WndProc via subclassing: just create with the correct proc
    // We need to register a class with our proc the first time
    static bool class_with_proc = false;
    if (!class_with_proc) {
        WNDCLASSEXA wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = silvertune_wnd_proc;
        wc.hInstance     = GetModuleHandleA(nullptr);
        wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
        wc.lpszClassName = WNDCLASS_NAME;
        wc.cbWndExtra    = sizeof(void *);
        RegisterClassExA(&wc); // may fail if already registered (that's OK)
        class_with_proc = true;
    }

    HWND hwnd = CreateWindowExA(
        0, WNDCLASS_NAME, "SilvertuneGUI",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, GUI_W, GUI_H,
        parent_hwnd, nullptr,
        GetModuleHandleA(nullptr), nullptr
    );

    if (!hwnd) return false;

    // Ensure our WndProc is set
    SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)silvertune_wnd_proc);
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)p);
    p->gui.handle = hwnd;

    SetTimer(hwnd, 1, 33, nullptr);
    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

void gui_set_scale(SilvertunePlugin *p, double scale) {
    p->gui.dpi_scale = scale;
}

void gui_show(SilvertunePlugin *p) {
    HWND hwnd = static_cast<HWND>(p->gui.handle);
    if (hwnd) ShowWindow(hwnd, SW_SHOW);
}

void gui_hide(SilvertunePlugin *p) {
    HWND hwnd = static_cast<HWND>(p->gui.handle);
    if (hwnd) ShowWindow(hwnd, SW_HIDE);
}

void gui_destroy(SilvertunePlugin *p) {
    if (!p->gui.created) return;
    p->gui.created = false;
    HWND hwnd = static_cast<HWND>(p->gui.handle);
    p->gui.handle = nullptr;
    if (hwnd) {
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
    }
}

#endif // _WIN32
