#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
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
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = WNDCLASS_NAME;
    wc.cbWndExtra    = sizeof(void *);

    if (!RegisterClassExA(&wc)) return false;
    registered = true;
    return true;
}

// ---------------------------------------------------------------------------
// GDI font management
// ---------------------------------------------------------------------------

static HFONT s_font_sm = nullptr;  // 10px — labels, small text
static HFONT s_font_md = nullptr;  // 12px — main labels, values
static HFONT s_font_lg = nullptr;  // 22px bold — note name

static void ensure_fonts() {
    if (s_font_sm) return;
    s_font_sm = CreateFontA(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    s_font_md = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    s_font_lg = CreateFontA(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

static COLORREF gc(const GuiColor &c) {
    return RGB(c.r, c.g, c.b);
}

// ---------------------------------------------------------------------------
// GDI text helpers — all text white, y is top of character cell
// ---------------------------------------------------------------------------

static void draw_str(HDC dc, int x, int y, const char *str, HFONT font) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    TextOutA(dc, x, y, str, (int)strlen(str));
    SelectObject(dc, old);
}

static void draw_str_c(HDC dc, int cx, int y, const char *str, HFONT font) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE sz = {};
    GetTextExtentPoint32A(dc, str, (int)strlen(str), &sz);
    SelectObject(dc, old);
    draw_str(dc, cx - sz.cx / 2, y, str, font);
}

static void draw_str_r(HDC dc, int x, int y, const char *str, HFONT font) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE sz = {};
    GetTextExtentPoint32A(dc, str, (int)strlen(str), &sz);
    SelectObject(dc, old);
    draw_str(dc, x - sz.cx, y, str, font);
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

static void draw_line_dc(HDC dc, int x1, int y1, int x2, int y2, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HPEN old = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
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
    LONG_PTR lp = GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    SilvertunePlugin *p = reinterpret_cast<SilvertunePlugin *>(lp);
    if (!p) return;

    // Double buffer
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, GUI_W, GUI_H);
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

    COLORREF col_bg         = gc(COL_BG);
    COLORREF col_display_bg = gc(COL_DISPLAY_BG);
    COLORREF col_accent     = gc(COL_ACCENT);
    COLORREF col_dim        = gc(COL_DIM);
    COLORREF col_label      = gc(COL_LABEL);
    COLORREF col_track      = gc(COL_TRACK);
    COLORREF col_fill       = gc(COL_FILL);
    COLORREF col_thumb      = gc(COL_THUMB);
    COLORREF col_white      = gc(COL_WHITE);
    COLORREF col_hdr_sep    = gc(COL_HDR_SEP);

    // Background
    fill_rect_dc(mem_dc, 0, 0, GUI_W, GUI_H, col_bg);

    // -----------------------------------------------------------------------
    // Header bar
    // -----------------------------------------------------------------------
    draw_str(mem_dc, 8, 4, "SILVERTUNE", s_font_md);
    draw_str_r(mem_dc, GUI_W - 8, 6, "VERTICAL RECTANGLE", s_font_sm);
    draw_line_dc(mem_dc, 0, HDR_H, GUI_W, HDR_H, col_hdr_sep);

    // -----------------------------------------------------------------------
    // OLED display panel
    // -----------------------------------------------------------------------
    draw_rect_dc(mem_dc, DISP_X, DISP_Y, DISP_W, DISP_H, col_dim);
    fill_rect_dc(mem_dc, DISP_X + 1, DISP_Y + 1, DISP_W - 2, DISP_H - 2, col_display_bg);

    // Read pitch state
    float dmidi  = p->gui_det_midi.load(std::memory_order_relaxed);
    int   corr   = p->gui_corr.load(std::memory_order_relaxed);
    bool  active = (corr >= 0 && dmidi >= 0.0f);

    // Piano keyboard
    {
        static const int WK[7] = { 0, 2, 4, 5, 7, 9, 11 };
        struct BkDef { int note, dx; };
        static const BkDef BK[5] = {
            {1, 19}, {3, 45}, {6, 97}, {8, 123}, {10, 149}
        };
        int hi = (corr >= 0) ? corr % 12 : -1;

        for (int i = 0; i < 7; ++i) {
            int x   = PIANO_KX + i * PIANO_WK_W;
            bool lit = (WK[i] == hi);
            if (lit)
                fill_rect_dc(mem_dc, x, PIANO_KY, PIANO_WK_W - 1, PIANO_WK_H, col_accent);
            else
                draw_rect_dc(mem_dc, x, PIANO_KY, PIANO_WK_W, PIANO_WK_H + 1, col_dim);
        }
        for (int i = 0; i < 5; ++i) {
            int x   = PIANO_KX + BK[i].dx;
            bool lit = (BK[i].note == hi);
            fill_rect_dc(mem_dc, x, PIANO_KY, PIANO_BK_W, PIANO_BK_H, lit ? col_accent : col_label);
        }
    }

    // Needle animation
    {
        uint32_t frame = p->gui_det_frame.load(std::memory_order_relaxed);
        if (!active) {
            p->gui.snap_cooldown = 0;
        } else if (frame != p->gui.last_det_frame) {
            p->gui.last_det_frame = frame;
            if (p->gui.snap_cooldown == 0) {
                float raw = (dmidi - (float)corr) * 100.0f;
                p->gui.disp_cents = raw > 50.0f ? 50.0f : (raw < -50.0f ? -50.0f : raw);
                p->gui.snap_cooldown = 8;
            }
        }
        if (p->gui.snap_cooldown > 0) --p->gui.snap_cooldown;
        float speed_norm = (float)p->param_speed.load() / 100.0f;
        float decay = 0.4f + speed_norm * 0.55f;
        p->gui.disp_cents *= decay;
    }
    float dc = p->gui.disp_cents;

    // Arc: upper semicircle as polyline
    {
        POINT pts[49];
        for (int i = 0; i <= 48; ++i) {
            double a = (double)i * M_PI / 48.0;
            pts[i].x = ARC_PCX + (int)(ARC_R * std::cos(a));
            pts[i].y = ARC_PCY - (int)(ARC_R * std::sin(a));
        }
        HPEN pen = CreatePen(PS_SOLID, 1, col_dim);
        HPEN old = (HPEN)SelectObject(mem_dc, pen);
        Polyline(mem_dc, pts, 49);
        SelectObject(mem_dc, old);
        DeleteObject(pen);
    }

    // Tick marks at 0, ±25, ±50 cents
    {
        struct { int cv; int len; } ticks[] = {
            {0, 12}, {-25, 7}, {25, 7}, {-50, 5}, {50, 5}
        };
        for (auto &tk : ticks) {
            double a  = (90.0 - tk.cv * 90.0 / 50.0) * M_PI / 180.0;
            double ca = std::cos(a), sa = std::sin(a);
            int ix = ARC_PCX + (int)((ARC_R - tk.len) * ca);
            int iy = ARC_PCY - (int)((ARC_R - tk.len) * sa);
            int ox = ARC_PCX + (int)((ARC_R + 3)      * ca);
            int oy = ARC_PCY - (int)((ARC_R + 3)      * sa);
            COLORREF tc = (tk.cv == 0) ? col_dim : col_label;
            HPEN pen = CreatePen(PS_SOLID, 1, tc);
            HPEN old = (HPEN)SelectObject(mem_dc, pen);
            MoveToEx(mem_dc, ix, iy, nullptr);
            LineTo(mem_dc, ox, oy);
            SelectObject(mem_dc, old);
            DeleteObject(pen);
        }
    }

    // Needle + pivot dot
    {
        double a  = (90.0 - (double)dc * 90.0 / 50.0) * M_PI / 180.0;
        int nx    = ARC_PCX + (int)((ARC_R - 5) * std::cos(a));
        int ny    = ARC_PCY - (int)((ARC_R - 5) * std::sin(a));
        COLORREF nc = active ? (std::fabs(dc) < 5.0f ? col_accent : col_white) : col_label;
        HPEN pen = CreatePen(PS_SOLID, 1, nc);
        HPEN old = (HPEN)SelectObject(mem_dc, pen);
        MoveToEx(mem_dc, ARC_PCX, ARC_PCY, nullptr);
        LineTo(mem_dc, nx, ny);
        SelectObject(mem_dc, old);
        DeleteObject(pen);
        fill_circle_dc(mem_dc, nx,      ny,      2, nc);
        fill_circle_dc(mem_dc, ARC_PCX, ARC_PCY, 2, col_dim);
    }

    // Corrected note name below pivot, large font
    {
        const char *corr_str = (corr >= 0) ? NOTE_NAMES[corr % 12] : "--";
        draw_str_c(mem_dc, ARC_PCX, ARC_PCY + 14, corr_str, s_font_lg);
    }

    // Cents value below note name — measure lg font height for offset
    {
        char cbuf[16];
        if (active) {
            float raw = (dmidi - (float)corr) * 100.0f;
            snprintf(cbuf, sizeof(cbuf), "%+.0fc", raw);
        } else {
            snprintf(cbuf, sizeof(cbuf), "--");
        }
        TEXTMETRIC tm = {};
        HFONT old = (HFONT)SelectObject(mem_dc, s_font_lg);
        GetTextMetrics(mem_dc, &tm);
        SelectObject(mem_dc, old);
        int cy = ARC_PCY + 14 + tm.tmHeight + 4;
        draw_str_c(mem_dc, ARC_PCX, cy, cbuf, s_font_sm);
    }

    // -----------------------------------------------------------------------
    // Right control panel separator
    // -----------------------------------------------------------------------
    draw_line_dc(mem_dc, DISP_X + DISP_W + 4, DISP_Y,
                 DISP_X + DISP_W + 4, DISP_Y + DISP_H, col_hdr_sep);

    // -----------------------------------------------------------------------
    // KEY stepper
    // -----------------------------------------------------------------------
    draw_str(mem_dc, KEY_LABEL_X, KEY_LABEL_Y, "KEY", s_font_sm);
    draw_left_triangle_dc(mem_dc, KEY_LEFT_X, KEY_BTN_Y, col_white);
    draw_right_triangle_dc(mem_dc, KEY_RIGHT_X, KEY_BTN_Y, col_white);
    {
        int key = (int)std::lround(p->param_key.load());
        key = ((key % 12) + 12) % 12;
        draw_str(mem_dc, KEY_TEXT_X, KEY_BTN_Y, NOTE_NAMES[key], s_font_md);
    }

    // -----------------------------------------------------------------------
    // SCALE stepper
    // -----------------------------------------------------------------------
    draw_str(mem_dc, SCALE_LABEL_X, SCALE_LABEL_Y, "SCALE", s_font_sm);
    draw_left_triangle_dc(mem_dc, SCALE_LEFT_X, SCALE_BTN_Y, col_white);
    draw_right_triangle_dc(mem_dc, SCALE_RIGHT_X, SCALE_BTN_Y, col_white);
    {
        int ps = (int)std::lround(p->param_scale.load());
        ps = ps < 0 ? 0 : (ps > 2 ? 2 : ps);
        int gs = PARAM_TO_GUI_SCALE[ps];
        draw_str(mem_dc, SCALE_TEXT_X, SCALE_BTN_Y, SCALE_NAMES_GUI[gs], s_font_md);
    }

    // Horizontal divider
    draw_line_dc(mem_dc, CTRL_X, KEY_BTN_Y + 20, GUI_W - 8, KEY_BTN_Y + 20, col_hdr_sep);

    // -----------------------------------------------------------------------
    // WIDE slider
    // -----------------------------------------------------------------------
    {
        float wide = (float)p->param_wide.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", wide * 100.0f);
        draw_str(mem_dc, WIDE_LABEL_X, WIDE_LABEL_Y, "WIDE", s_font_sm);
        draw_str_r(mem_dc, WIDE_PCT_X, WIDE_PCT_Y, pct, s_font_sm);
        fill_rect_dc(mem_dc, WIDE_TRACK_X, WIDE_TRACK_Y, WIDE_TRACK_W, WIDE_TRACK_H, col_track);
        int fw = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W) - WIDE_TRACK_X;
        if (fw > 0) fill_rect_dc(mem_dc, WIDE_TRACK_X, WIDE_TRACK_Y, fw, WIDE_TRACK_H, col_fill);
        int tx = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W);
        int ty = WIDE_TRACK_Y + WIDE_TRACK_H / 2;
        fill_circle_dc(mem_dc, tx, ty, SLIDER_THUMB_R, col_thumb);
    }

    // -----------------------------------------------------------------------
    // SPEED slider
    // -----------------------------------------------------------------------
    {
        float speed_ms = (float)p->param_speed.load();
        char val[16];
        if (speed_ms <= 0.0f) snprintf(val, sizeof(val), "0ms");
        else snprintf(val, sizeof(val), "%.0fms", speed_ms);
        draw_str(mem_dc, SPEED_LABEL_X, SPEED_LABEL_Y, "SPEED", s_font_sm);
        draw_str_r(mem_dc, SPEED_PCT_X, SPEED_PCT_Y, val, s_font_sm);
        fill_rect_dc(mem_dc, SPEED_TRACK_X, SPEED_TRACK_Y, SPEED_TRACK_W, SPEED_TRACK_H, col_track);
        float speed_norm = speed_ms / 100.0f;
        int fw = slider_px(speed_norm, SPEED_TRACK_X, SPEED_TRACK_W) - SPEED_TRACK_X;
        if (fw > 0) fill_rect_dc(mem_dc, SPEED_TRACK_X, SPEED_TRACK_Y, fw, SPEED_TRACK_H, col_fill);
        int tx = slider_px(speed_norm, SPEED_TRACK_X, SPEED_TRACK_W);
        int ty = SPEED_TRACK_Y + SPEED_TRACK_H / 2;
        fill_circle_dc(mem_dc, tx, ty, SLIDER_THUMB_R, col_thumb);
    }

    // -----------------------------------------------------------------------
    // HOLD slider
    // -----------------------------------------------------------------------
    {
        float hold_ms = (float)p->param_hold.load();
        char val[16];
        if (hold_ms <= 0.0f) snprintf(val, sizeof(val), "0ms");
        else snprintf(val, sizeof(val), "%.0fms", hold_ms);
        draw_str(mem_dc, HOLD_LABEL_X, HOLD_LABEL_Y, "HOLD", s_font_sm);
        draw_str_r(mem_dc, HOLD_PCT_X, HOLD_PCT_Y, val, s_font_sm);
        fill_rect_dc(mem_dc, HOLD_TRACK_X, HOLD_TRACK_Y, HOLD_TRACK_W, HOLD_TRACK_H, col_track);
        float hold_norm = hold_ms / 200.0f;
        int fw = slider_px(hold_norm, HOLD_TRACK_X, HOLD_TRACK_W) - HOLD_TRACK_X;
        if (fw > 0) fill_rect_dc(mem_dc, HOLD_TRACK_X, HOLD_TRACK_Y, fw, HOLD_TRACK_H, col_fill);
        int tx = slider_px(hold_norm, HOLD_TRACK_X, HOLD_TRACK_W);
        int ty = HOLD_TRACK_Y + HOLD_TRACK_H / 2;
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
    case PARAM_HOLD:  p->param_hold.store(value);  break;
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

        {
            int semi = hit_piano_key(mx, my);
            if (semi >= 0) {
                float hz = midi_to_hz(60.0f + semi);
                p->preview_phase = 0.0;
                p->gui_preview_hz.store(hz, std::memory_order_relaxed);
                p->gui_preview_frames.store((int)(p->sample_rate * 0.5), std::memory_order_relaxed);
                return 0;
            }
        }

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
            p->gui.drag_wide = true; p->gui.drag_speed = false; p->gui.drag_hold = false;
            p->gui.drag_x0   = mx;
            p->gui.drag_v0   = (float)p->param_wide.load();
            set_param_and_notify(p, PARAM_WIDE, slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (hit_speed_track(mx, my)) {
            p->gui.drag_speed = true; p->gui.drag_wide = false; p->gui.drag_hold = false;
            p->gui.drag_x0    = mx;
            p->gui.drag_v0    = (float)p->param_speed.load();
            set_param_and_notify(p, PARAM_SPEED, slider_val(mx, SPEED_TRACK_X, SPEED_TRACK_W) * 100.0);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (hit_hold_track(mx, my)) {
            p->gui.drag_hold = true; p->gui.drag_wide = false; p->gui.drag_speed = false;
            p->gui.drag_x0   = mx;
            p->gui.drag_v0   = (float)p->param_hold.load();
            set_param_and_notify(p, PARAM_HOLD, slider_val(mx, HOLD_TRACK_X, HOLD_TRACK_W) * 200.0);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!p) return 0;
        int mx = LOWORD(lp);
        if (p->gui.drag_wide) {
            set_param_and_notify(p, PARAM_WIDE, slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W));
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (p->gui.drag_speed) {
            set_param_and_notify(p, PARAM_SPEED, slider_val(mx, SPEED_TRACK_X, SPEED_TRACK_W) * 100.0);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (p->gui.drag_hold) {
            set_param_and_notify(p, PARAM_HOLD, slider_val(mx, HOLD_TRACK_X, HOLD_TRACK_W) * 200.0);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (p) {
            p->gui.drag_wide  = false;
            p->gui.drag_speed = false;
            p->gui.drag_hold  = false;
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
    ensure_fonts();
    p->gui.created = true;
}

bool gui_set_parent(SilvertunePlugin *p, void *native_parent) {
    if (!p->gui.created) return false;

    HWND parent_hwnd = static_cast<HWND>(native_parent);

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
        RegisterClassExA(&wc);
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
