#if !defined(_WIN32) && !defined(__APPLE__)

#include "gui.h"
#include "silvertune.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
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
    XFontStruct    *fonts[3]  = {};   // [0]=label 10pt, [1]=value 12pt, [2]=note 18pt
    bool            running   = false;
    pthread_t       thread;
    unsigned long   colors[9] = {};   // indexed by enum below
    SilvertunePlugin *plugin  = nullptr;

    // Pipe for wakeup/stop signalling
    int wake_fd[2] = {-1, -1};
};

enum ColorIdx {
    C_BG = 0, C_DISPLAY_BG, C_GREEN, C_DIM_GREEN,
    C_LABEL, C_TRACK, C_FILL, C_THUMB, C_WHITE
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
    // Fallback: find nearest
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

static void draw_text(X11Data *d, Drawable drw, int x, int y, const char *text, int font_idx, int cidx) {
    if (!text || !d->fonts[font_idx]) return;
    XSetFont(d->dpy, d->gc, d->fonts[font_idx]->fid);
    set_color(d, cidx);
    // y is baseline in X11
    XDrawString(d->dpy, drw, d->gc, x, y, text, (int)strlen(text));
}

static void fill_circle(X11Data *d, Drawable drw, int cx, int cy, int r, int cidx) {
    set_color(d, cidx);
    XFillArc(d->dpy, drw, d->gc, cx - r, cy - r, (unsigned)(r * 2), (unsigned)(r * 2), 0, 360 * 64);
}

// Draw a small left-pointing triangle (< button)
static void draw_left_triangle(X11Data *d, Drawable drw, int bx, int by, int cidx) {
    // Triangle pointing left, inside button box STEPPER_BTN_W x STEPPER_BTN_H
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

static int font_height(X11Data *d, int font_idx) {
    if (!d->fonts[font_idx]) return 12;
    return d->fonts[font_idx]->ascent + d->fonts[font_idx]->descent;
}

static int font_ascent(X11Data *d, int font_idx) {
    if (!d->fonts[font_idx]) return 10;
    return d->fonts[font_idx]->ascent;
}

static int text_width(X11Data *d, int font_idx, const char *text) {
    if (!d->fonts[font_idx] || !text) return 0;
    return XTextWidth(d->fonts[font_idx], text, (int)strlen(text));
}

// ---------------------------------------------------------------------------
// Main draw function — draws to the double-buffer Pixmap then copies to win
// ---------------------------------------------------------------------------

static void do_draw(X11Data *d) {
    SilvertunePlugin *p = d->plugin;
    Pixmap buf = d->buf;

    // Background
    fill_rect(d, buf, 0, 0, GUI_W, GUI_H, C_BG);

    // --- Left display panel (OLED arc needle meter) ---
    draw_rect(d, buf, DISP_X, DISP_Y, DISP_W, DISP_H, C_DIM_GREEN);
    fill_rect(d, buf, DISP_X + 1, DISP_Y + 1, DISP_W - 2, DISP_H - 2, C_DISPLAY_BG);

    const int pcx   = DISP_X + DISP_W / 2;   // pivot x = 82
    const int pcy   = DISP_Y + DISP_H - 60;  // pivot y = 132
    const int arc_r = 54;

    // Title centered
    {
        const char *title = "SILVERTUNE";
        int tw = text_width(d, 0, title);
        draw_text(d, buf, pcx - tw / 2, DISP_Y + 5 + font_ascent(d, 0), title, 0, C_GREEN);
    }

    // Separator
    int sep_y = DISP_Y + 5 + font_height(d, 0) + 3;
    draw_line(d, buf, DISP_X + 2, sep_y, DISP_X + DISP_W - 3, sep_y, C_DIM_GREEN);

    // Read pitch state (shared by piano, needle, and CORR display)
    float dmidi  = p->gui_det_midi.load(std::memory_order_relaxed);
    int   corr   = p->gui_corr.load(std::memory_order_relaxed);
    bool  active = (corr >= 0 && dmidi >= 0.0f);

    // Mini piano: one octave C-B, highlight corrected note class
    {
        static const int WK[7] = { 0, 2, 4, 5, 7, 9, 11 };
        struct BkDef { int note, dx; };
        static const BkDef BK[5] = {
            {1, 14}, {3, 34}, {6, 74}, {8, 94}, {10, 114}
        };
        int hi = (corr >= 0) ? corr % 12 : -1;

        for (int i = 0; i < 7; ++i) {
            int x   = PIANO_KX + i * PIANO_WK_W;
            bool lit = (WK[i] == hi);
            if (lit)
                fill_rect(d, buf, x, PIANO_KY, PIANO_WK_W - 1, PIANO_WK_H, C_GREEN);
            else
                draw_rect(d, buf, x, PIANO_KY, PIANO_WK_W, PIANO_WK_H + 1, C_DIM_GREEN);
        }
        for (int i = 0; i < 5; ++i) {
            int x   = PIANO_KX + BK[i].dx;
            bool lit = (BK[i].note == hi);
            fill_rect(d, buf, x, PIANO_KY, PIANO_BK_W, PIANO_BK_H, lit ? C_GREEN : C_LABEL);
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
        float tune  = (float)p->param_speed.load();
        float decay = 0.4f + (1.0f - tune) * 0.55f;
        p->gui.disp_cents *= decay;
    }
    float dc = p->gui.disp_cents;

    // Arc: upper semicircle as polyline (a=0 right → a=π left, through top)
    for (int i = 0; i < 48; ++i) {
        double a0 = (double)i       * M_PI / 48.0;
        double a1 = (double)(i + 1) * M_PI / 48.0;
        int x0 = pcx + (int)(arc_r * std::cos(a0));
        int y0 = pcy - (int)(arc_r * std::sin(a0));
        int x1 = pcx + (int)(arc_r * std::cos(a1));
        int y1 = pcy - (int)(arc_r * std::sin(a1));
        draw_line(d, buf, x0, y0, x1, y1, C_DIM_GREEN);
    }

    // Tick marks at 0, ±25, ±50 cents
    {
        struct { int cv; int len; } ticks[] = {
            {0, 12}, {-25, 7}, {25, 7}, {-50, 5}, {50, 5}
        };
        for (auto &tk : ticks) {
            double a  = (90.0 - tk.cv * 90.0 / 50.0) * M_PI / 180.0;
            double ca = std::cos(a), sa = std::sin(a);
            int ix = pcx + (int)((arc_r - tk.len) * ca);
            int iy = pcy - (int)((arc_r - tk.len) * sa);
            int ox = pcx + (int)((arc_r + 3)      * ca);
            int oy = pcy - (int)((arc_r + 3)      * sa);
            draw_line(d, buf, ix, iy, ox, oy, tk.cv == 0 ? C_DIM_GREEN : C_LABEL);
        }
    }

    // Needle from pivot to arc, smoothed
    {
        double a  = (90.0 - (double)dc * 90.0 / 50.0) * M_PI / 180.0;
        int nx    = pcx + (int)((arc_r - 5) * std::cos(a));
        int ny    = pcy - (int)((arc_r - 5) * std::sin(a));
        int ncol  = active ? (std::fabs(dc) < 5.0f ? C_GREEN : C_WHITE) : C_LABEL;
        draw_line(d, buf, pcx, pcy, nx, ny, ncol);
        fill_circle(d, buf, nx,  ny,  2, ncol);
        fill_circle(d, buf, pcx, pcy, 2, C_DIM_GREEN);
    }

    // CORR note below pivot
    {
        const char *corr_str = (corr >= 0) ? NOTE_NAMES[corr % 12] : "--";
        int nw = text_width(d, 1, corr_str);
        draw_text(d, buf, pcx - nw / 2, pcy + 14 + font_ascent(d, 1), corr_str, 1, C_WHITE);
    }

    // Cents value below CORR note
    if (active) {
        float raw = (dmidi - (float)corr) * 100.0f;
        char cbuf[16];
        snprintf(cbuf, sizeof(cbuf), "%+.0fc", raw);
        int cw   = text_width(d, 0, cbuf);
        int ccol = std::fabs(dc) < 5.0f ? C_GREEN : C_LABEL;
        draw_text(d, buf, pcx - cw / 2,
                  pcy + 14 + font_height(d, 1) + 4 + font_ascent(d, 0),
                  cbuf, 0, ccol);
    }

    // --- KEY stepper ---
    draw_text(d, buf, KEY_LABEL_X, KEY_LABEL_Y + font_ascent(d, 0), "KEY", 0, C_LABEL);

    draw_left_triangle(d, buf, KEY_LEFT_X, KEY_BTN_Y, C_LABEL);
    draw_right_triangle(d, buf, KEY_RIGHT_X, KEY_BTN_Y, C_LABEL);

    {
        int key = (int)std::lround(p->param_key.load());
        key = ((key % 12) + 12) % 12;
        const char *key_str = NOTE_NAMES[key];
        draw_text(d, buf, KEY_TEXT_X, KEY_BTN_Y + font_ascent(d, 1), key_str, 1, C_GREEN);
    }

    // --- SCALE stepper ---
    draw_text(d, buf, SCALE_LABEL_X, SCALE_LABEL_Y + font_ascent(d, 0), "SCALE", 0, C_LABEL);

    draw_left_triangle(d, buf, SCALE_LEFT_X, SCALE_BTN_Y, C_LABEL);
    draw_right_triangle(d, buf, SCALE_RIGHT_X, SCALE_BTN_Y, C_LABEL);

    {
        int param_scale = (int)std::lround(p->param_scale.load());
        int gui_scale = PARAM_TO_GUI_SCALE[param_scale < 0 ? 0 : (param_scale > 2 ? 2 : param_scale)];
        const char *scale_str = SCALE_NAMES_GUI[gui_scale];
        draw_text(d, buf, SCALE_TEXT_X, SCALE_BTN_Y + font_ascent(d, 1), scale_str, 1, C_GREEN);
    }

    // --- WIDE slider ---
    {
        float wide = (float)p->param_wide.load();
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%.0f%%", wide * 100.0f);

        draw_text(d, buf, WIDE_LABEL_X, WIDE_LABEL_Y + font_ascent(d, 0), "WIDE", 0, C_LABEL);
        // Right-align percent text
        int pw = text_width(d, 1, pct_str);
        draw_text(d, buf, WIDE_PCT_X - pw, WIDE_PCT_Y + font_ascent(d, 1), pct_str, 1, C_LABEL);

        // Track
        fill_rect(d, buf, WIDE_TRACK_X, WIDE_TRACK_Y, WIDE_TRACK_W, WIDE_TRACK_H, C_TRACK);
        // Fill
        int fill_w = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W) - WIDE_TRACK_X;
        if (fill_w > 0)
            fill_rect(d, buf, WIDE_TRACK_X, WIDE_TRACK_Y, fill_w, WIDE_TRACK_H, C_FILL);
        // Thumb
        int thumb_x = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W);
        int thumb_y = WIDE_TRACK_Y + WIDE_TRACK_H / 2;
        fill_circle(d, buf, thumb_x, thumb_y, SLIDER_THUMB_R, C_THUMB);
    }

    // --- TUNE slider ---
    {
        float tune = (float)p->param_speed.load();
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%.0f%%", tune * 100.0f);

        draw_text(d, buf, TUNE_LABEL_X, TUNE_LABEL_Y + font_ascent(d, 0), "TUNE", 0, C_LABEL);
        int pw = text_width(d, 1, pct_str);
        draw_text(d, buf, TUNE_PCT_X - pw, TUNE_PCT_Y + font_ascent(d, 1), pct_str, 1, C_LABEL);

        // Track
        fill_rect(d, buf, TUNE_TRACK_X, TUNE_TRACK_Y, TUNE_TRACK_W, TUNE_TRACK_H, C_TRACK);
        // Fill
        int fill_w = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W) - TUNE_TRACK_X;
        if (fill_w > 0)
            fill_rect(d, buf, TUNE_TRACK_X, TUNE_TRACK_Y, fill_w, TUNE_TRACK_H, C_FILL);
        // Thumb
        int thumb_x = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W);
        int thumb_y = TUNE_TRACK_Y + TUNE_TRACK_H / 2;
        fill_circle(d, buf, thumb_x, thumb_y, SLIDER_THUMB_R, C_THUMB);
    }

    // Copy pixmap to window
    XCopyArea(d->dpy, d->buf, d->win, d->gc, 0, 0, GUI_W, GUI_H, 0, 0);
    XFlush(d->dpy);
}

// ---------------------------------------------------------------------------
// Parameter change helper: store to atomic + request callback flush
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
        do_draw(d);
        return;
    }
    if (hit_key_right(mx, my)) {
        int k = (int)std::lround(p->param_key.load());
        k = (k + 1) % 12;
        set_param_and_notify(p, PARAM_KEY, (double)k);
        do_draw(d);
        return;
    }
    if (hit_scale_left(mx, my)) {
        int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
        s = ((s - 1) % 3 + 3) % 3;
        set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
        do_draw(d);
        return;
    }
    if (hit_scale_right(mx, my)) {
        int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
        s = (s + 1) % 3;
        set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
        do_draw(d);
        return;
    }
    if (hit_wide_track(mx, my)) {
        p->gui.drag_wide = true;
        p->gui.drag_tune = false;
        p->gui.drag_x0   = mx;
        p->gui.drag_v0   = (float)p->param_wide.load();
        float v = slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W);
        set_param_and_notify(p, PARAM_WIDE, (double)v);
        do_draw(d);
        return;
    }
    if (hit_tune_track(mx, my)) {
        p->gui.drag_tune = true;
        p->gui.drag_wide = false;
        p->gui.drag_x0   = mx;
        p->gui.drag_v0   = (float)p->param_speed.load();
        float v = slider_val(mx, TUNE_TRACK_X, TUNE_TRACK_W);
        set_param_and_notify(p, PARAM_SPEED, (double)v);
        do_draw(d);
        return;
    }
}

static void handle_motion(X11Data *d, int mx) {
    SilvertunePlugin *p = d->plugin;
    if (p->gui.drag_wide) {
        float v = slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W);
        set_param_and_notify(p, PARAM_WIDE, (double)v);
        do_draw(d);
    } else if (p->gui.drag_tune) {
        float v = slider_val(mx, TUNE_TRACK_X, TUNE_TRACK_W);
        set_param_and_notify(p, PARAM_SPEED, (double)v);
        do_draw(d);
    }
}

static void handle_button_release(X11Data *d) {
    d->plugin->gui.drag_wide = false;
    d->plugin->gui.drag_tune = false;
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
        tv.tv_sec  = 0;
        tv.tv_usec = 33000; // ~30 fps timer

        int ret = select(nfds, &rfds, nullptr, nullptr, &tv);

        if (!d->running) break;

        if (ret < 0) break;

        // Drain wake pipe
        if (ret > 0 && FD_ISSET(wake_r, &rfds)) {
            char buf[64];
            (void)read(wake_r, buf, sizeof(buf));
        }

        // Handle X11 events
        while (XPending(d->dpy)) {
            XEvent ev;
            XNextEvent(d->dpy, &ev);
            switch (ev.type) {
            case Expose:
                if (ev.xexpose.count == 0)
                    do_draw(d);
                break;
            case ButtonPress:
                if (ev.xbutton.button == Button1)
                    handle_button_press(d, ev.xbutton.x, ev.xbutton.y);
                break;
            case MotionNotify:
                handle_motion(d, ev.xmotion.x);
                break;
            case ButtonRelease:
                if (ev.xbutton.button == Button1)
                    handle_button_release(d);
                break;
            default:
                break;
            }
        }

        // Periodic redraw (~30fps on timeout)
        if (ret == 0) {
            do_draw(d);
        }
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
    if (!d->dpy) {
        delete d;
        p->gui.handle = nullptr;
        return false;
    }

    int screen = DefaultScreen(d->dpy);
    Window parent_win = native_parent
        ? static_cast<Window>(reinterpret_cast<unsigned long>(native_parent))
        : DefaultRootWindow(d->dpy);

    XSetWindowAttributes wa;
    wa.background_pixel = BlackPixel(d->dpy, screen);
    wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask | StructureNotifyMask;

    d->win = XCreateWindow(
        d->dpy, parent_win,
        0, 0, GUI_W, GUI_H, 0,
        DefaultDepth(d->dpy, screen),
        InputOutput,
        DefaultVisual(d->dpy, screen),
        CWBackPixel | CWEventMask,
        &wa
    );

    d->gc  = XCreateGC(d->dpy, d->win, 0, nullptr);
    d->buf = XCreatePixmap(d->dpy, d->win, GUI_W, GUI_H,
                            DefaultDepth(d->dpy, screen));

    // Load fonts
    const char *font_specs[3][3] = {
        // label (~10pt)
        { "-*-courier-medium-r-*-*-12-*-*-*-*-*-*-*",
          "-*-fixed-medium-r-*-*-12-*-*-*-*-*-*-*",
          "fixed" },
        // value (~12pt)
        { "-*-courier-medium-r-*-*-14-*-*-*-*-*-*-*",
          "-*-fixed-medium-r-*-*-14-*-*-*-*-*-*-*",
          "fixed" },
        // note (~18pt)
        { "-*-courier-bold-r-*-*-20-*-*-*-*-*-*-*",
          "-*-fixed-bold-r-*-*-20-*-*-*-*-*-*-*",
          "fixed" },
    };
    for (int fi = 0; fi < 3; ++fi) {
        for (int si = 0; si < 3; ++si) {
            d->fonts[fi] = XLoadQueryFont(d->dpy, font_specs[fi][si]);
            if (d->fonts[fi]) break;
        }
    }

    // Alloc colors
    d->colors[C_BG]         = alloc_color(d->dpy, screen, COL_BG.r,         COL_BG.g,         COL_BG.b);
    d->colors[C_DISPLAY_BG] = alloc_color(d->dpy, screen, COL_DISPLAY_BG.r, COL_DISPLAY_BG.g, COL_DISPLAY_BG.b);
    d->colors[C_GREEN]      = alloc_color(d->dpy, screen, COL_GREEN.r,       COL_GREEN.g,       COL_GREEN.b);
    d->colors[C_DIM_GREEN]  = alloc_color(d->dpy, screen, COL_DIM_GREEN.r,   COL_DIM_GREEN.g,   COL_DIM_GREEN.b);
    d->colors[C_LABEL]      = alloc_color(d->dpy, screen, COL_LABEL.r,       COL_LABEL.g,       COL_LABEL.b);
    d->colors[C_TRACK]      = alloc_color(d->dpy, screen, COL_TRACK.r,       COL_TRACK.g,       COL_TRACK.b);
    d->colors[C_FILL]       = alloc_color(d->dpy, screen, COL_FILL.r,        COL_FILL.g,        COL_FILL.b);
    d->colors[C_THUMB]      = alloc_color(d->dpy, screen, COL_THUMB.r,       COL_THUMB.g,       COL_THUMB.b);
    d->colors[C_WHITE]      = alloc_color(d->dpy, screen, COL_WHITE.r,       COL_WHITE.g,       COL_WHITE.b);

    XMapWindow(d->dpy, d->win);
    XFlush(d->dpy);

    // Wake pipe
    pipe(d->wake_fd);

    d->running = true;
    pthread_create(&d->thread, nullptr, x11_thread, d);

    return true;
}

void gui_set_scale(SilvertunePlugin *p, double scale) {
    p->gui.dpi_scale = scale;
}

void gui_show(SilvertunePlugin *p) {
    auto *d = static_cast<X11Data *>(p->gui.handle);
    if (!d) return;
    XMapWindow(d->dpy, d->win);
    XFlush(d->dpy);
}

void gui_hide(SilvertunePlugin *p) {
    auto *d = static_cast<X11Data *>(p->gui.handle);
    if (!d) return;
    XUnmapWindow(d->dpy, d->win);
    XFlush(d->dpy);
}

void gui_destroy(SilvertunePlugin *p) {
    if (!p->gui.created) return;
    p->gui.created = false;

    auto *d = static_cast<X11Data *>(p->gui.handle);
    p->gui.handle = nullptr;
    if (!d) return;

    // Signal thread to stop
    d->running = false;
    if (d->wake_fd[1] >= 0) {
        char c = 0;
        (void)write(d->wake_fd[1], &c, 1);
    }
    pthread_join(d->thread, nullptr);

    // Close pipe
    if (d->wake_fd[0] >= 0) close(d->wake_fd[0]);
    if (d->wake_fd[1] >= 0) close(d->wake_fd[1]);

    // Free fonts
    for (int i = 0; i < 3; ++i) {
        if (d->fonts[i])
            XFreeFont(d->dpy, d->fonts[i]);
    }

    XFreePixmap(d->dpy, d->buf);
    XFreeGC(d->dpy, d->gc);
    XDestroyWindow(d->dpy, d->win);
    XCloseDisplay(d->dpy);

    delete d;
}

#endif // !WIN32 && !APPLE
