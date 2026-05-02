#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#include "gui.h"
#include "silvertune.h"
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static inline void cgfill(CGContextRef ctx, CGFloat x, CGFloat y, CGFloat w, CGFloat h,
                           CGFloat r, CGFloat g, CGFloat b) {
    CGContextSetRGBFillColor(ctx, r, g, b, 1.0);
    CGContextFillRect(ctx, CGRectMake(x, y, w, h));
}

static inline void cgrect(CGContextRef ctx, CGFloat x, CGFloat y, CGFloat w, CGFloat h,
                           CGFloat r, CGFloat g, CGFloat b) {
    CGContextSetRGBStrokeColor(ctx, r, g, b, 1.0);
    CGContextSetLineWidth(ctx, 1.0);
    CGContextStrokeRect(ctx, CGRectMake(x + 0.5, y + 0.5, w - 1, h - 1));
}

static inline void cgline(CGContextRef ctx, CGFloat x1, CGFloat y1, CGFloat x2, CGFloat y2,
                           CGFloat r, CGFloat g, CGFloat b) {
    CGContextSetRGBStrokeColor(ctx, r, g, b, 1.0);
    CGContextSetLineWidth(ctx, 1.0);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, x1, y1);
    CGContextAddLineToPoint(ctx, x2, y2);
    CGContextStrokePath(ctx);
}

static inline void cgcircle(CGContextRef ctx, CGFloat cx, CGFloat cy, CGFloat r,
                             CGFloat red, CGFloat green, CGFloat blue) {
    CGContextSetRGBFillColor(ctx, red, green, blue, 1.0);
    CGContextFillEllipseInRect(ctx, CGRectMake(cx - r, cy - r, r * 2, r * 2));
}

static inline void cg_left_triangle(CGContextRef ctx, CGFloat bx, CGFloat by,
                                     CGFloat r, CGFloat g, CGFloat b) {
    CGFloat bw = STEPPER_BTN_W;
    CGFloat bh = STEPPER_BTN_H;
    CGContextSetRGBFillColor(ctx, r, g, b, 1.0);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx,    bx + bw - 3, by + 3);
    CGContextAddLineToPoint(ctx, bx + 3,      by + bh / 2.0);
    CGContextAddLineToPoint(ctx, bx + bw - 3, by + bh - 3);
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);
}

static inline void cg_right_triangle(CGContextRef ctx, CGFloat bx, CGFloat by,
                                      CGFloat r, CGFloat g, CGFloat b) {
    CGFloat bw = STEPPER_BTN_W;
    CGFloat bh = STEPPER_BTN_H;
    CGContextSetRGBFillColor(ctx, r, g, b, 1.0);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx,    bx + 3,      by + 3);
    CGContextAddLineToPoint(ctx, bx + bw - 3, by + bh / 2.0);
    CGContextAddLineToPoint(ctx, bx + 3,      by + bh - 3);
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);
}

// NSString text helpers — all text white, y is top of glyph cell (flipped view)
static void mac_str(const char *s, CGFloat x, CGFloat y, NSFont *font) {
    NSDictionary *a = @{ NSFontAttributeName: font,
                         NSForegroundColorAttributeName: [NSColor whiteColor] };
    [@(s) drawAtPoint:NSMakePoint(x, y) withAttributes:a];
}

static void mac_str_c(const char *s, CGFloat cx, CGFloat y, NSFont *font) {
    NSDictionary *a = @{ NSFontAttributeName: font,
                         NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSSize sz = [@(s) sizeWithAttributes:a];
    [@(s) drawAtPoint:NSMakePoint(cx - sz.width / 2.0, y) withAttributes:a];
}

static void mac_str_r(const char *s, CGFloat x, CGFloat y, NSFont *font) {
    NSDictionary *a = @{ NSFontAttributeName: font,
                         NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSSize sz = [@(s) sizeWithAttributes:a];
    [@(s) drawAtPoint:NSMakePoint(x - sz.width, y) withAttributes:a];
}

// ---------------------------------------------------------------------------
// SilvertuneView
// ---------------------------------------------------------------------------

@interface SilvertuneView : NSView {
    SilvertunePlugin *_plugin;
    NSTimer          *_timer;
    NSFont           *_font_sm;  // 10pt — labels, small text
    NSFont           *_font_md;  // 12pt — main labels, values
    NSFont           *_font_lg;  // 22pt bold — note name
}
- (instancetype)initWithPlugin:(SilvertunePlugin *)plugin;
@end

@implementation SilvertuneView

- (instancetype)initWithPlugin:(SilvertunePlugin *)plugin {
    self = [super initWithFrame:NSMakeRect(0, 0, GUI_W, GUI_H)];
    if (self) {
        _plugin  = plugin;
        _font_sm = [[NSFont systemFontOfSize:10] retain];
        _font_md = [[NSFont systemFontOfSize:12] retain];
        _font_lg = [[NSFont boldSystemFontOfSize:22] retain];
        _timer   = [NSTimer scheduledTimerWithTimeInterval:1.0/30.0
                                                    target:self
                                                  selector:@selector(timerFired:)
                                                  userInfo:nil
                                                   repeats:YES];
    }
    return self;
}

- (void)dealloc {
    [_timer invalidate];
    _timer = nil;
    [_font_sm release]; _font_sm = nil;
    [_font_md release]; _font_md = nil;
    [_font_lg release]; _font_lg = nil;
    [super dealloc];
}

- (BOOL)isFlipped { return YES; }

- (void)timerFired:(NSTimer *)t {
    [self setNeedsDisplay:YES];
}

// ---------------------------------------------------------------------------
// drawRect: — draws the full UI
// ---------------------------------------------------------------------------

- (void)drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    if (!ctx) return;

    SilvertunePlugin *p = _plugin;

#define R(c) ((c).r / 255.0)
#define G(c) ((c).g / 255.0)
#define B(c) ((c).b / 255.0)

    // Background
    cgfill(ctx, 0, 0, GUI_W, GUI_H, R(COL_BG), G(COL_BG), B(COL_BG));

    // -----------------------------------------------------------------------
    // Header bar
    // -----------------------------------------------------------------------
    mac_str("SILVERTUNE", 8, 4, _font_md);
    mac_str_r("VERTICAL RECTANGLE", GUI_W - 8, 6, _font_sm);
    cgline(ctx, 0, HDR_H, GUI_W, HDR_H, R(COL_HDR_SEP), G(COL_HDR_SEP), B(COL_HDR_SEP));

    // -----------------------------------------------------------------------
    // OLED display panel
    // -----------------------------------------------------------------------
    cgrect(ctx, DISP_X, DISP_Y, DISP_W, DISP_H,
           R(COL_DIM), G(COL_DIM), B(COL_DIM));
    cgfill(ctx, DISP_X + 1, DISP_Y + 1, DISP_W - 2, DISP_H - 2,
           R(COL_DISPLAY_BG), G(COL_DISPLAY_BG), B(COL_DISPLAY_BG));

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
            CGFloat x = PIANO_KX + i * PIANO_WK_W;
            bool lit  = (WK[i] == hi);
            if (lit)
                cgfill(ctx, x, PIANO_KY, PIANO_WK_W - 1, PIANO_WK_H,
                       R(COL_ACCENT), G(COL_ACCENT), B(COL_ACCENT));
            else
                cgrect(ctx, x, PIANO_KY, PIANO_WK_W, PIANO_WK_H + 1,
                       R(COL_DIM), G(COL_DIM), B(COL_DIM));
        }
        for (int i = 0; i < 5; ++i) {
            CGFloat x = PIANO_KX + BK[i].dx;
            bool lit  = (BK[i].note == hi);
            if (lit)
                cgfill(ctx, x, PIANO_KY, PIANO_BK_W, PIANO_BK_H,
                       R(COL_ACCENT), G(COL_ACCENT), B(COL_ACCENT));
            else
                cgfill(ctx, x, PIANO_KY, PIANO_BK_W, PIANO_BK_H,
                       R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
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

    // Arc: upper semicircle
    CGContextSetRGBStrokeColor(ctx, R(COL_DIM), G(COL_DIM), B(COL_DIM), 1.0);
    CGContextSetLineWidth(ctx, 1.0);
    CGContextBeginPath(ctx);
    for (int i = 0; i <= 48; ++i) {
        double a = (double)i * M_PI / 48.0;
        CGFloat x = ARC_PCX + (CGFloat)(ARC_R * std::cos(a));
        CGFloat y = ARC_PCY - (CGFloat)(ARC_R * std::sin(a));
        if (i == 0) CGContextMoveToPoint(ctx, x, y);
        else        CGContextAddLineToPoint(ctx, x, y);
    }
    CGContextStrokePath(ctx);

    // Tick marks at 0, ±25, ±50 cents
    {
        struct { int cv; int len; } ticks[] = {
            {0, 12}, {-25, 7}, {25, 7}, {-50, 5}, {50, 5}
        };
        for (auto &tk : ticks) {
            double a  = (90.0 - tk.cv * 90.0 / 50.0) * M_PI / 180.0;
            double ca = std::cos(a), sa = std::sin(a);
            CGFloat ix = ARC_PCX + (CGFloat)((ARC_R - tk.len) * ca);
            CGFloat iy = ARC_PCY - (CGFloat)((ARC_R - tk.len) * sa);
            CGFloat ox = ARC_PCX + (CGFloat)((ARC_R + 3)      * ca);
            CGFloat oy = ARC_PCY - (CGFloat)((ARC_R + 3)      * sa);
            if (tk.cv == 0)
                cgline(ctx, ix, iy, ox, oy, R(COL_DIM), G(COL_DIM), B(COL_DIM));
            else
                cgline(ctx, ix, iy, ox, oy, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        }
    }

    // Needle from pivot to arc
    {
        double a   = (90.0 - (double)dc * 90.0 / 50.0) * M_PI / 180.0;
        CGFloat nx = ARC_PCX + (CGFloat)((ARC_R - 5) * std::cos(a));
        CGFloat ny = ARC_PCY - (CGFloat)((ARC_R - 5) * std::sin(a));
        bool in_tune = active && std::fabs(dc) < 5.0f;
        if (!active)
            cgline(ctx, ARC_PCX, ARC_PCY, nx, ny, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        else if (in_tune)
            cgline(ctx, ARC_PCX, ARC_PCY, nx, ny, R(COL_ACCENT), G(COL_ACCENT), B(COL_ACCENT));
        else
            cgline(ctx, ARC_PCX, ARC_PCY, nx, ny, 1.0, 1.0, 1.0);

        CGFloat nr = active ? (in_tune ? R(COL_ACCENT) : 1.0) : R(COL_LABEL);
        CGFloat ng = active ? (in_tune ? G(COL_ACCENT) : 1.0) : G(COL_LABEL);
        CGFloat nb = active ? (in_tune ? B(COL_ACCENT) : 1.0) : B(COL_LABEL);
        cgcircle(ctx, nx,      ny,      2.0, nr, ng, nb);
        cgcircle(ctx, ARC_PCX, ARC_PCY, 2.0, R(COL_DIM), G(COL_DIM), B(COL_DIM));
    }

    // Note name below pivot, large font
    {
        const char *corr_str = (corr >= 0) ? NOTE_NAMES[corr % 12] : "--";
        mac_str_c(corr_str, ARC_PCX, ARC_PCY + 16, _font_lg);
    }

    // Cents value below note name
    {
        char cbuf[16];
        if (active) {
            float raw = (dmidi - (float)corr) * 100.0f;
            snprintf(cbuf, sizeof(cbuf), "%+.0fc", raw);
        } else {
            snprintf(cbuf, sizeof(cbuf), "--");
        }
        CGFloat lg_h = _font_lg.ascender + std::fabs((double)_font_lg.descender);
        mac_str_c(cbuf, ARC_PCX, ARC_PCY + 16 + lg_h + 4, _font_sm);
    }

    // -----------------------------------------------------------------------
    // Right control panel separator
    // -----------------------------------------------------------------------
    cgline(ctx, DISP_X + DISP_W + 4, DISP_Y,
               DISP_X + DISP_W + 4, DISP_Y + DISP_H,
           R(COL_HDR_SEP), G(COL_HDR_SEP), B(COL_HDR_SEP));

    // -----------------------------------------------------------------------
    // KEY stepper
    // -----------------------------------------------------------------------
    mac_str("KEY", KEY_LABEL_X, KEY_LABEL_Y, _font_sm);
    cg_left_triangle(ctx,  KEY_LEFT_X,  KEY_BTN_Y, 1.0, 1.0, 1.0);
    cg_right_triangle(ctx, KEY_RIGHT_X, KEY_BTN_Y, 1.0, 1.0, 1.0);
    {
        int key = (int)std::lround(p->param_key.load());
        key = ((key % 12) + 12) % 12;
        mac_str(NOTE_NAMES[key], KEY_TEXT_X, KEY_BTN_Y, _font_md);
    }

    // -----------------------------------------------------------------------
    // SCALE stepper
    // -----------------------------------------------------------------------
    mac_str("SCALE", SCALE_LABEL_X, SCALE_LABEL_Y, _font_sm);
    cg_left_triangle(ctx,  SCALE_LEFT_X,  SCALE_BTN_Y, 1.0, 1.0, 1.0);
    cg_right_triangle(ctx, SCALE_RIGHT_X, SCALE_BTN_Y, 1.0, 1.0, 1.0);
    {
        int ps = (int)std::lround(p->param_scale.load());
        ps = ps < 0 ? 0 : (ps > 2 ? 2 : ps);
        int gs = PARAM_TO_GUI_SCALE[ps];
        mac_str(SCALE_NAMES_GUI[gs], SCALE_TEXT_X, SCALE_BTN_Y, _font_md);
    }

    cgline(ctx, CTRL_X, KEY_BTN_Y + 22, GUI_W - 8, KEY_BTN_Y + 22,
           R(COL_HDR_SEP), G(COL_HDR_SEP), B(COL_HDR_SEP));

    // -----------------------------------------------------------------------
    // WIDE slider
    // -----------------------------------------------------------------------
    {
        float wide = (float)p->param_wide.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", wide * 100.0f);
        mac_str("WIDE", WIDE_LABEL_X, WIDE_LABEL_Y, _font_sm);
        mac_str_r(pct, WIDE_PCT_X, WIDE_PCT_Y, _font_sm);
        cgfill(ctx, WIDE_TRACK_X, WIDE_TRACK_Y, WIDE_TRACK_W, WIDE_TRACK_H,
               R(COL_TRACK), G(COL_TRACK), B(COL_TRACK));
        int fw = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W) - WIDE_TRACK_X;
        if (fw > 0)
            cgfill(ctx, WIDE_TRACK_X, WIDE_TRACK_Y, fw, WIDE_TRACK_H,
                   R(COL_FILL), G(COL_FILL), B(COL_FILL));
        int tx = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W);
        cgcircle(ctx, tx, WIDE_TRACK_Y + WIDE_TRACK_H / 2, SLIDER_THUMB_R,
                 R(COL_THUMB), G(COL_THUMB), B(COL_THUMB));
    }

    // -----------------------------------------------------------------------
    // TUNE slider
    // -----------------------------------------------------------------------
    {
        float tune = (float)p->param_speed.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", tune * 100.0f);
        mac_str("TUNE", TUNE_LABEL_X, TUNE_LABEL_Y, _font_sm);
        mac_str_r(pct, TUNE_PCT_X, TUNE_PCT_Y, _font_sm);
        cgfill(ctx, TUNE_TRACK_X, TUNE_TRACK_Y, TUNE_TRACK_W, TUNE_TRACK_H,
               R(COL_TRACK), G(COL_TRACK), B(COL_TRACK));
        int fw = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W) - TUNE_TRACK_X;
        if (fw > 0)
            cgfill(ctx, TUNE_TRACK_X, TUNE_TRACK_Y, fw, TUNE_TRACK_H,
                   R(COL_FILL), G(COL_FILL), B(COL_FILL));
        int tx = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W);
        cgcircle(ctx, tx, TUNE_TRACK_Y + TUNE_TRACK_H / 2, SLIDER_THUMB_R,
                 R(COL_THUMB), G(COL_THUMB), B(COL_THUMB));
    }

#undef R
#undef G
#undef B
}

// ---------------------------------------------------------------------------
// Mouse events
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

- (void)mouseDown:(NSEvent *)event {
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    int mx = (int)pt.x;
    int my = (int)pt.y;
    SilvertunePlugin *p = _plugin;

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
        [self setNeedsDisplay:YES];
        return;
    }
    if (hit_key_right(mx, my)) {
        int k = (int)std::lround(p->param_key.load());
        k = (k + 1) % 12;
        set_param_and_notify(p, PARAM_KEY, (double)k);
        [self setNeedsDisplay:YES];
        return;
    }
    if (hit_scale_left(mx, my)) {
        int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
        s = ((s - 1) % 3 + 3) % 3;
        set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
        [self setNeedsDisplay:YES];
        return;
    }
    if (hit_scale_right(mx, my)) {
        int s = PARAM_TO_GUI_SCALE[(int)std::lround(p->param_scale.load())];
        s = (s + 1) % 3;
        set_param_and_notify(p, PARAM_SCALE, (double)GUI_SCALE_TO_PARAM[s]);
        [self setNeedsDisplay:YES];
        return;
    }
    if (hit_wide_track(mx, my)) {
        p->gui.drag_wide = true;
        p->gui.drag_tune = false;
        p->gui.drag_x0   = mx;
        p->gui.drag_v0   = (float)p->param_wide.load();
        float v = slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W);
        set_param_and_notify(p, PARAM_WIDE, (double)v);
        [self setNeedsDisplay:YES];
        return;
    }
    if (hit_tune_track(mx, my)) {
        p->gui.drag_tune = true;
        p->gui.drag_wide = false;
        p->gui.drag_x0   = mx;
        p->gui.drag_v0   = (float)p->param_speed.load();
        float v = slider_val(mx, TUNE_TRACK_X, TUNE_TRACK_W);
        set_param_and_notify(p, PARAM_SPEED, (double)v);
        [self setNeedsDisplay:YES];
        return;
    }
}

- (void)mouseDragged:(NSEvent *)event {
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    int mx = (int)pt.x;
    SilvertunePlugin *p = _plugin;
    if (p->gui.drag_wide) {
        float v = slider_val(mx, WIDE_TRACK_X, WIDE_TRACK_W);
        set_param_and_notify(p, PARAM_WIDE, (double)v);
        [self setNeedsDisplay:YES];
    } else if (p->gui.drag_tune) {
        float v = slider_val(mx, TUNE_TRACK_X, TUNE_TRACK_W);
        set_param_and_notify(p, PARAM_SPEED, (double)v);
        [self setNeedsDisplay:YES];
    }
}

- (void)mouseUp:(NSEvent *)event {
    _plugin->gui.drag_wide = false;
    _plugin->gui.drag_tune = false;
}

@end

// ---------------------------------------------------------------------------
// Platform GUI API
// ---------------------------------------------------------------------------

void gui_create(SilvertunePlugin *p) {
    p->gui.created = true;
}

bool gui_set_parent(SilvertunePlugin *p, void *native_parent) {
    if (!p->gui.created) return false;

    NSView *parent_view = (__bridge NSView *)native_parent;
    if (!parent_view) return false;

    SilvertuneView *view = [[SilvertuneView alloc] initWithPlugin:p];
    [parent_view addSubview:view];
    p->gui.handle = (__bridge_retained void *)view;
    return true;
}

void gui_set_scale(SilvertunePlugin *p, double scale) {
    p->gui.dpi_scale = scale;
}

void gui_show(SilvertunePlugin *p) {
    SilvertuneView *view = (__bridge SilvertuneView *)p->gui.handle;
    if (view) [view setHidden:NO];
}

void gui_hide(SilvertunePlugin *p) {
    SilvertuneView *view = (__bridge SilvertuneView *)p->gui.handle;
    if (view) [view setHidden:YES];
}

void gui_destroy(SilvertunePlugin *p) {
    if (!p->gui.created) return;
    p->gui.created = false;

    if (p->gui.handle) {
        SilvertuneView *view = (__bridge_transfer SilvertuneView *)p->gui.handle;
        p->gui.handle = nullptr;
        [view removeFromSuperview];
        view = nil;
    }
}

#endif // __APPLE__
