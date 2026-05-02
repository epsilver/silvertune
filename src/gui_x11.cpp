#if !defined(_WIN32) && !defined(__APPLE__)

#include "gui.h"
#include "silvertune.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Internal per-instance state
// ---------------------------------------------------------------------------

struct X11Data {
    Display        *dpy       = nullptr;
    Window          win       = 0;
    GC              gc        = 0;
    Pixmap          buf       = 0;
    bool            running   = false;
    pthread_t       thread;
    unsigned long   colors[10] = {};
    SilvertunePlugin *plugin  = nullptr;
    int             wake_fd[2] = {-1, -1};
    int             screen    = 0;
    // Xft
    XftFont        *xft_sm    = nullptr;  // 10px
    XftFont        *xft_md    = nullptr;  // 12px
    XftFont        *xft_lg    = nullptr;  // 22px bold
    XftDraw        *xft_draw  = nullptr;
    XftColor        xft_white = {};
};

enum ColorIdx {
    C_BG = 0, C_DISPLAY_BG, C_ACCENT, C_DIM,
    C_LABEL, C_TRACK, C_FILL, C_THUMB, C_WHITE, C_HDR_SEP
};

// ---------------------------------------------------------------------------
// Color allocation
// ---------------------------------------------------------------------------

static unsigned long alloc_color(Display *dpy, int screen, uint8_t r, uint8_t g, uint8_t b) {
    XColor xc;
    xc.red   = (unsigned short)(r * 257);
    xc.green = (unsigned short)(g * 257);
    xc.blue  = (unsigned short)(b * 257);
    xc.flags = DoRed | DoGreen | DoBlue;
    Colormap cmap = DefaultColormap(dpy, screen);
    if (XAllocColor(dpy, cmap, &xc))
        return xc.pixel;
    return BlackPixel(dpy, screen);
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void set_color(X11Data *d, int cidx) {
    XSetForeground(d->dpy, d->gc, d->colors[cidx]);
}

static void fill_rect(X11Data *d, Drawable drw, int x, int y, int w, int h, int cidx) {
    set_color(d, cidx);
    XFillRectangle(d->dpy, drw, d->gc, x, y, (unsigned)w, (unsigned)h);
}

static void draw_rect(X11Data *d, Drawable drw, int x, int y, int w, int h, int cidx) {
    set_color(d, cidx);
    XDrawRectangle(d->dpy, drw, d->gc, x, y, (unsigned)(w - 1), (unsigned)(h - 1));
}

static void draw_line(X11Data *d, Drawable drw, int x1, int y1, int x2, int y2, int cidx) {
    set_color(d, cidx);
    XDrawLine(d->dpy, drw, d->gc, x1, y1, x2, y2);
}

static void fill_circle(X11Data *d, Drawable drw, int cx, int cy, int r, int cidx) {
    set_color(d, cidx);
    XFillArc(d->dpy, drw, d->gc, cx - r, cy - r, (unsigned)(r * 2), (unsigned)(r * 2), 0, 360 * 64);
}

static void draw_left_triangle(X11Data *d, Drawable drw, int bx, int by, int cidx) {
    XPoint pts[3];
    pts[0].x = bx + STEPPER_BTN_W - 3;  pts[0].y = by + 3;
    pts[1].x = bx + 3;                   pts[1].y = by + STEPPER_BTN_H / 2;
    pts[2].x = bx + STEPPER_BTN_W - 3;  pts[2].y = by + STEPPER_BTN_H - 3;
    set_color(d, cidx);
    XFillPolygon(d->dpy, drw, d->gc, pts, 3, Convex, CoordModeOrigin);
}

static void draw_right_triangle(X11Data *d, Drawable drw, int bx, int by, int cidx) {
    XPoint pts[3];
    pts[0].x = bx + 3;                   pts[0].y = by + 3;
    pts[1].x = bx + STEPPER_BTN_W - 3;  pts[1].y = by + STEPPER_BTN_H / 2;
    pts[2].x = bx + 3;                   pts[2].y = by + STEPPER_BTN_H - 3;
    set_color(d, cidx);
    XFillPolygon(d->dpy, drw, d->gc, pts, 3, Convex, CoordModeOrigin);
}

// ---------------------------------------------------------------------------
// Xft text helpers — y is top of text cell, not baseline
// ---------------------------------------------------------------------------

static int xft_width(X11Data *d, XftFont *f, const char *str) {
    XGlyphInfo ext;
    XftTextExtents8(d->dpy, f, (const FcChar8 *)str, (int)strlen(str), &ext);
    return ext.xOff;
}

static void draw_str(X11Data *d, int x, int y, const char *str, XftFont *f) {
    XftDrawString8(d->xft_draw, &d->xft_white, f,
                   x, y + f->ascent,
                   (const FcChar8 *)str, (int)strlen(str));
}

static void draw_str_c(X11Data *d, int cx, int y, const char *str, XftFont *f) {
    draw_str(d, cx - xft_width(d, f, str) / 2, y, str, f);
}

static void draw_str_r(X11Data *d, int x, int y, const char *str, XftFont *f) {
    draw_str(d, x - xft_width(d, f, str), y, str, f);
}

// ---------------------------------------------------------------------------
// Main draw function
// ---------------------------------------------------------------------------

static void do_draw(X11Data *d) {
    SilvertunePlugin *p = d->plugin;
    Pixmap buf = d->buf;

    fill_rect(d, buf, 0, 0, GUI_W, GUI_H, C_BG);

    // -----------------------------------------------------------------------
    // Header bar
    // -----------------------------------------------------------------------
    draw_str(d, 8, 4, "SILVERTUNE", d->xft_md);
    draw_str_r(d, GUI_W - 8, 6, "VERTICAL RECTANGLE", d->xft_sm);
    draw_line(d, buf, 0, HDR_H, GUI_W, HDR_H, C_HDR_SEP);

    // -----------------------------------------------------------------------
    // OLED display panel
    // -----------------------------------------------------------------------
    draw_rect(d, buf, DISP_X, DISP_Y, DISP_W, DISP_H, C_DIM);
    fill_rect(d, buf, DISP_X + 1, DISP_Y + 1, DISP_W - 2, DISP_H - 2, C_DISPLAY_BG);

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
                fill_rect(d, buf, x, PIANO_KY, PIANO_WK_W - 1, PIANO_WK_H, C_ACCENT);
            else
                draw_rect(d, buf, x, PIANO_KY, PIANO_WK_W, PIANO_WK_H + 1, C_DIM);
        }
        for (int i = 0; i < 5; ++i) {
            int x   = PIANO_KX + BK[i].dx;
            bool lit = (BK[i].note == hi);
            fill_rect(d, buf, x, PIANO_KY, PIANO_BK_W, PIANO_BK_H, lit ? C_ACCENT : C_LABEL);
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

    // Arc
    for (int i = 0; i < 48; ++i) {
        double a0 = (double)i       * M_PI / 48.0;
        double a1 = (double)(i + 1) * M_PI / 48.0;
        int x0 = ARC_PCX + (int)(ARC_R * std::cos(a0));
        int y0 = ARC_PCY - (int)(ARC_R * std::sin(a0));
        int x1 = ARC_PCX + (int)(ARC_R * std::cos(a1));
        int y1 = ARC_PCY - (int)(ARC_R * std::sin(a1));
        draw_line(d, buf, x0, y0, x1, y1, C_DIM);
    }

    // Tick marks
    {
        struct { int cv; int len; } ticks[] = {
            {0, 14}, {-25, 8}, {25, 8}, {-50, 6}, {50, 6}
        };
        for (auto &tk : ticks) {
            double a  = (90.0 - tk.cv * 90.0 / 50.0) * M_PI / 180.0;
            double ca = std::cos(a), sa = std::sin(a);
            int ix = ARC_PCX + (int)((ARC_R - tk.len) * ca);
            int iy = ARC_PCY - (int)((ARC_R - tk.len) * sa);
            int ox = ARC_PCX + (int)((ARC_R + 4)      * ca);
            int oy = ARC_PCY - (int)((ARC_R + 4)      * sa);
            draw_line(d, buf, ix, iy, ox, oy, tk.cv == 0 ? C_DIM : C_LABEL);
        }
    }

    // Needle
    {
        double a  = (90.0 - (double)dc * 90.0 / 50.0) * M_PI / 180.0;
        int nx    = ARC_PCX + (int)((ARC_R - 6) * std::cos(a));
        int ny    = ARC_PCY - (int)((ARC_R - 6) * std::sin(a));
        int ncol  = active ? (std::fabs(dc) < 5.0f ? C_ACCENT : C_WHITE) : C_LABEL;
        draw_line(d, buf, ARC_PCX, ARC_PCY, nx, ny, ncol);
        fill_circle(d, buf, nx,      ny,      3, ncol);
        fill_circle(d, buf, ARC_PCX, ARC_PCY, 3, C_DIM);
    }

    // Note name (large, centered under pivot)
    {
        const char *corr_str = (corr >= 0) ? NOTE_NAMES[corr % 12] : "--";
        draw_str_c(d, ARC_PCX, ARC_PCY + 16, corr_str, d->xft_lg);
    }

    // Cents value
    {
        char cbuf[16];
        if (active) {
            float raw = (dmidi - (float)corr) * 100.0f;
            snprintf(cbuf, sizeof(cbuf), "%+.0fc", raw);
        } else {
            snprintf(cbuf, sizeof(cbuf), "--");
        }
        int cy = ARC_PCY + 16 + d->xft_lg->height + 4;
        draw_str_c(d, ARC_PCX, cy, cbuf, d->xft_sm);
    }

    // -----------------------------------------------------------------------
    // Right control panel separator
    // -----------------------------------------------------------------------
    draw_line(d, buf, DISP_X + DISP_W + 4, DISP_Y,
              DISP_X + DISP_W + 4, DISP_Y + DISP_H, C_HDR_SEP);

    // -----------------------------------------------------------------------
    // KEY stepper
    // -----------------------------------------------------------------------
    draw_str(d, KEY_LABEL_X, KEY_LABEL_Y, "KEY", d->xft_sm);
    draw_left_triangle(d, buf, KEY_LEFT_X, KEY_BTN_Y, C_WHITE);
    draw_right_triangle(d, buf, KEY_RIGHT_X, KEY_BTN_Y, C_WHITE);
    {
        int key = (int)std::lround(p->param_key.load());
        key = ((key % 12) + 12) % 12;
        draw_str(d, KEY_TEXT_X, KEY_BTN_Y, NOTE_NAMES[key], d->xft_md);
    }

    // -----------------------------------------------------------------------
    // SCALE stepper
    // -----------------------------------------------------------------------
    draw_str(d, SCALE_LABEL_X, SCALE_LABEL_Y, "SCALE", d->xft_sm);
    draw_left_triangle(d, buf, SCALE_LEFT_X, SCALE_BTN_Y, C_WHITE);
    draw_right_triangle(d, buf, SCALE_RIGHT_X, SCALE_BTN_Y, C_WHITE);
    {
        int ps = (int)std::lround(p->param_scale.load());
        ps = ps < 0 ? 0 : (ps > 2 ? 2 : ps);
        int gs = PARAM_TO_GUI_SCALE[ps];
        draw_str(d, SCALE_TEXT_X, SCALE_BTN_Y, SCALE_NAMES_GUI[gs], d->xft_md);
    }

    draw_line(d, buf, CTRL_X, KEY_BTN_Y + 22, GUI_W - 8, KEY_BTN_Y + 22, C_HDR_SEP);

    // -----------------------------------------------------------------------
    // WIDE slider
    // -----------------------------------------------------------------------
    {
        float wide = (float)p->param_wide.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", wide * 100.0f);
        draw_str(d, WIDE_LABEL_X, WIDE_LABEL_Y, "WIDE", d->xft_sm);
        draw_str_r(d, WIDE_PCT_X, WIDE_PCT_Y, pct, d->xft_sm);
        fill_rect(d, buf, WIDE_TRACK_X, WIDE_TRACK_Y, WIDE_TRACK_W, WIDE_TRACK_H, C_TRACK);
        int fw = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W) - WIDE_TRACK_X;
        if (fw > 0) fill_rect(d, buf, WIDE_TRACK_X, WIDE_TRACK_Y, fw, WIDE_TRACK_H, C_FILL);
        int tx = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W);
        fill_circle(d, buf, tx, WIDE_TRACK_Y + WIDE_TRACK_H / 2, SLIDER_THUMB_R, C_THUMB);
    }

    // -----------------------------------------------------------------------
    // SPEED slider
    // -----------------------------------------------------------------------
    {
        float speed_ms = (float)p->param_speed.load();
        char val[16];
        if (speed_ms <= 0.0f) snprintf(val, sizeof(val), "0ms");
        else snprintf(val, sizeof(val), "%.0fms", speed_ms);
        draw_str(d, SPEED_LABEL_X, SPEED_LABEL_Y, "SPEED", d->xft_sm);
        draw_str_r(d, SPEED_PCT_X, SPEED_PCT_Y, val, d->xft_sm);
        fill_rect(d, buf, SPEED_TRACK_X, SPEED_TRACK_Y, SPEED_TRACK_W, SPEED_TRACK_H, C_TRACK);
        float speed_norm = speed_ms / 100.0f;
        int fw = slider_px(speed_norm, SPEED_TRACK_X, SPEED_TRACK_W) - SPEED_TRACK_X;
        if (fw > 0) fill_rect(d, buf, SPEED_TRACK_X, SPEED_TRACK_Y, fw, SPEED_TRACK_H, C_FILL);
        int tx = slider_px(speed_norm, SPEED_TRACK_X, SPEED_TRACK_W);
        fill_circle(d, buf, tx, SPEED_TRACK_Y + SPEED_TRACK_H / 2, SLIDER_THUMB_R, C_THUMB);
    }

    // -----------------------------------------------------------------------
    // HOLD slider
    // -----------------------------------------------------------------------
    {
        float hold_ms = (float)p->param_hold.load();
        char val[16];
        if (hold_ms <= 0.0f) snprintf(val, sizeof(val), "0ms");
        else snprintf(val, sizeof(val), "%.0fms", hold_ms);
        draw_str(d, HOLD_LABEL_X, HOLD_LABEL_Y, "HOLD", d->xft_sm);
        draw_str_r(d, HOLD_PCT_X, HOLD_PCT_Y, val, d->xft_sm);
        fill_rect(d, buf, HOLD_TRACK_X, HOLD_TRACK_Y, HOLD_TRACK_W, HOLD_TRACK_H, C_TRACK);
        float hold_norm = hold_ms / 200.0f;
        int fw = slider_px(hold_norm, HOLD_TRACK_X, HOLD_TRACK_W) - HOLD_TRACK_X;
        if (fw > 0) fill_rect(d, buf, HOLD_TRACK_X, HOLD_TRACK_Y, fw, HOLD_TRACK_H, C_FILL);
        int tx = slider_px(hold_norm, HOLD_TRACK_X, HOLD_TRACK_W);
        fill_circle(d, buf, tx, HOLD_TRACK_Y + HOLD_TRACK_H / 2, SLIDER_THUMB_R, C_THUMB);
    }

    XCopyArea(d->dpy, d->buf, d->win, d->gc, 0, 0, GUI_W, GUI_H, 0, 0);
    XFlush(d->dpy);
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
// Event handling
// ---------------------------------------------------------------------------

static void handle_button_press(X11Data *d, int mx, int my) {
    SilvertunePlugin *p = d->plugin;

    {
        int semi = hit_piano_key(mx, my);
        if (semi >= 0) {
            float hz = midi_to_hz(60.0f + semi);
            p->preview_phase = 0.0;
            p->gui_preview_hz.store(hz, std::memory_order_relaxed);
            p->gui_preview_frames.store((int)(p->sample_rate * 0.5), std::memory_order_relaxed);
            return;
        }
    }

    if (hit_key_left(mx, my)) {
        int k = (int)std::lround(p->param_key.load());
        k = ((k - 1) % 12 + 12) % 12;
        set_param_and_notify(p, PARAM_KEY, (double)k);
        do_draw(d); return;
    }
    if (hit_key_right(mx, my)) {
        int k = (int)std::lround(p->param_key.load());
        k = (k + 1) % 12;
        set_param_and_notify(p, PARAM_KEY, (double)k);
        do_draw(d); return;
    }
    if (hit_scale_left(mx, my)) {
        int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
        s = ((s - 1) % 3 + 3) % 3;
        set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
        do_draw(d); return;
    }
    if (hit_scale_right(mx, my)) {
        int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
        s = (s + 1) % 3;
        set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
        do_draw(d); return;
    }
    if (hit_wide_track(mx, my)) {
        p->gui.drag_wide = true; p->gui.drag_speed = false; p->gui.drag_hold = false;
        p->gui.drag_x0 = mx; p->gui.drag_v0 = (float)p->param_wide.load();
        set_param_and_notify(p, PARAM_WIDE, (double)slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W));
        do_draw(d); return;
    }
    if (hit_speed_track(mx, my)) {
        p->gui.drag_speed = true; p->gui.drag_wide = false; p->gui.drag_hold = false;
        p->gui.drag_x0 = mx; p->gui.drag_v0 = (float)p->param_speed.load();
        set_param_and_notify(p, PARAM_SPEED, slider_val(mx, SPEED_TRACK_X, SPEED_TRACK_W) * 100.0);
        do_draw(d); return;
    }
    if (hit_hold_track(mx, my)) {
        p->gui.drag_hold = true; p->gui.drag_wide = false; p->gui.drag_speed = false;
        p->gui.drag_x0 = mx; p->gui.drag_v0 = (float)p->param_hold.load();
        set_param_and_notify(p, PARAM_HOLD, slider_val(mx, HOLD_TRACK_X, HOLD_TRACK_W) * 200.0);
        do_draw(d); return;
    }
}

static void handle_motion(X11Data *d, int mx) {
    SilvertunePlugin *p = d->plugin;
    if (p->gui.drag_wide) {
        set_param_and_notify(p, PARAM_WIDE, (double)slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W));
        do_draw(d);
    } else if (p->gui.drag_speed) {
        set_param_and_notify(p, PARAM_SPEED, slider_val(mx, SPEED_TRACK_X, SPEED_TRACK_W) * 100.0);
        do_draw(d);
    } else if (p->gui.drag_hold) {
        set_param_and_notify(p, PARAM_HOLD, slider_val(mx, HOLD_TRACK_X, HOLD_TRACK_W) * 200.0);
        do_draw(d);
    }
}

static void handle_button_release(X11Data *d) {
    d->plugin->gui.drag_wide  = false;
    d->plugin->gui.drag_speed = false;
    d->plugin->gui.drag_hold  = false;
}

// ---------------------------------------------------------------------------
// Background thread
// ---------------------------------------------------------------------------

static void *x11_thread(void *arg) {
    X11Data *d = static_cast<X11Data *>(arg);
    int x11_fd = ConnectionNumber(d->dpy);
    int wake_r = d->wake_fd[0];

    do_draw(d);

    while (d->running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(x11_fd, &rfds);
        FD_SET(wake_r, &rfds);
        int nfds = (x11_fd > wake_r ? x11_fd : wake_r) + 1;

        struct timeval tv;
        tv.tv_sec = 0; tv.tv_usec = 33000;

        int ret = select(nfds, &rfds, nullptr, nullptr, &tv);
        if (!d->running) break;
        if (ret < 0)  break;

        if (ret > 0 && FD_ISSET(wake_r, &rfds)) {
            char buf[64];
            (void)read(wake_r, buf, sizeof(buf));
        }

        while (XPending(d->dpy)) {
            XEvent ev;
            XNextEvent(d->dpy, &ev);
            switch (ev.type) {
            case Expose:
                if (ev.xexpose.count == 0) do_draw(d);
                break;
            case ButtonPress:
                if (ev.xbutton.button == Button1)
                    handle_button_press(d, ev.xbutton.x, ev.xbutton.y);
                break;
            case MotionNotify:
                handle_motion(d, ev.xmotion.x);
                break;
            case ButtonRelease:
                if (ev.xbutton.button == Button1) handle_button_release(d);
                break;
            default: break;
            }
        }

        if (ret == 0) do_draw(d);
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Platform GUI API
// ---------------------------------------------------------------------------

void gui_create(SilvertunePlugin *p) {
    p->gui.created = true;
}

bool gui_set_parent(SilvertunePlugin *p, void *native_parent) {
    if (!p->gui.created) return false;

    auto *d = new X11Data();
    d->plugin = p;
    p->gui.handle = d;

    d->dpy = XOpenDisplay(nullptr);
    if (!d->dpy) { delete d; p->gui.handle = nullptr; return false; }

    d->screen = DefaultScreen(d->dpy);
    Window parent_win = native_parent
        ? static_cast<Window>(reinterpret_cast<unsigned long>(native_parent))
        : DefaultRootWindow(d->dpy);

    XSetWindowAttributes wa;
    wa.background_pixel = BlackPixel(d->dpy, d->screen);
    wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask | StructureNotifyMask;

    d->win = XCreateWindow(d->dpy, parent_win, 0, 0, GUI_W, GUI_H, 0,
                            DefaultDepth(d->dpy, d->screen), InputOutput,
                            DefaultVisual(d->dpy, d->screen),
                            CWBackPixel | CWEventMask, &wa);

    d->gc  = XCreateGC(d->dpy, d->win, 0, nullptr);
    d->buf = XCreatePixmap(d->dpy, d->win, GUI_W, GUI_H,
                            DefaultDepth(d->dpy, d->screen));

    d->colors[C_BG]         = alloc_color(d->dpy, d->screen, COL_BG.r,         COL_BG.g,         COL_BG.b);
    d->colors[C_DISPLAY_BG] = alloc_color(d->dpy, d->screen, COL_DISPLAY_BG.r, COL_DISPLAY_BG.g, COL_DISPLAY_BG.b);
    d->colors[C_ACCENT]     = alloc_color(d->dpy, d->screen, COL_ACCENT.r,     COL_ACCENT.g,     COL_ACCENT.b);
    d->colors[C_DIM]        = alloc_color(d->dpy, d->screen, COL_DIM.r,        COL_DIM.g,        COL_DIM.b);
    d->colors[C_LABEL]      = alloc_color(d->dpy, d->screen, COL_LABEL.r,      COL_LABEL.g,      COL_LABEL.b);
    d->colors[C_TRACK]      = alloc_color(d->dpy, d->screen, COL_TRACK.r,      COL_TRACK.g,      COL_TRACK.b);
    d->colors[C_FILL]       = alloc_color(d->dpy, d->screen, COL_FILL.r,       COL_FILL.g,       COL_FILL.b);
    d->colors[C_THUMB]      = alloc_color(d->dpy, d->screen, COL_THUMB.r,      COL_THUMB.g,      COL_THUMB.b);
    d->colors[C_WHITE]      = alloc_color(d->dpy, d->screen, COL_WHITE.r,      COL_WHITE.g,      COL_WHITE.b);
    d->colors[C_HDR_SEP]    = alloc_color(d->dpy, d->screen, COL_HDR_SEP.r,    COL_HDR_SEP.g,    COL_HDR_SEP.b);

    // Open Xft fonts
    d->xft_sm = XftFontOpen(d->dpy, d->screen,
        XFT_FAMILY, XftTypeString, "sans",
        XFT_PIXEL_SIZE, XftTypeDouble, 10.0,
        NULL);
    d->xft_md = XftFontOpen(d->dpy, d->screen,
        XFT_FAMILY, XftTypeString, "sans",
        XFT_PIXEL_SIZE, XftTypeDouble, 12.0,
        NULL);
    d->xft_lg = XftFontOpen(d->dpy, d->screen,
        XFT_FAMILY, XftTypeString, "sans",
        XFT_WEIGHT, XftTypeInteger, 200,  // FC_WEIGHT_BOLD
        XFT_PIXEL_SIZE, XftTypeDouble, 22.0,
        NULL);

    // XftDraw bound to the offscreen pixmap
    d->xft_draw = XftDrawCreate(d->dpy, d->buf,
                                DefaultVisual(d->dpy, d->screen),
                                DefaultColormap(d->dpy, d->screen));

    d->xft_white.pixel        = d->colors[C_WHITE];
    d->xft_white.color.red    = 0xFFFF;
    d->xft_white.color.green  = 0xFFFF;
    d->xft_white.color.blue   = 0xFFFF;
    d->xft_white.color.alpha  = 0xFFFF;

    XMapWindow(d->dpy, d->win);
    XFlush(d->dpy);
    pipe(d->wake_fd);
    d->running = true;
    pthread_create(&d->thread, nullptr, x11_thread, d);
    return true;
}

void gui_set_scale(SilvertunePlugin *p, double scale) { p->gui.dpi_scale = scale; }

void gui_show(SilvertunePlugin *p) {
    auto *d = static_cast<X11Data *>(p->gui.handle);
    if (!d) return;
    XMapWindow(d->dpy, d->win); XFlush(d->dpy);
}

void gui_hide(SilvertunePlugin *p) {
    auto *d = static_cast<X11Data *>(p->gui.handle);
    if (!d) return;
    XUnmapWindow(d->dpy, d->win); XFlush(d->dpy);
}

void gui_destroy(SilvertunePlugin *p) {
    if (!p->gui.created) return;
    p->gui.created = false;

    auto *d = static_cast<X11Data *>(p->gui.handle);
    p->gui.handle = nullptr;
    if (!d) return;

    d->running = false;
    if (d->wake_fd[1] >= 0) { char c = 0; (void)write(d->wake_fd[1], &c, 1); }
    pthread_join(d->thread, nullptr);

    if (d->wake_fd[0] >= 0) close(d->wake_fd[0]);
    if (d->wake_fd[1] >= 0) close(d->wake_fd[1]);

    if (d->xft_draw) XftDrawDestroy(d->xft_draw);
    if (d->xft_sm)   XftFontClose(d->dpy, d->xft_sm);
    if (d->xft_md)   XftFontClose(d->dpy, d->xft_md);
    if (d->xft_lg)   XftFontClose(d->dpy, d->xft_lg);

    XFreePixmap(d->dpy, d->buf);
    XFreeGC(d->dpy, d->gc);
    XDestroyWindow(d->dpy, d->win);
    XCloseDisplay(d->dpy);
    delete d;
}

#endif // !WIN32 && !APPLE
