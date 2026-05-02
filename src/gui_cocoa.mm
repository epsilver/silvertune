#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#include <CoreText/CoreText.h>
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

// Draw a triangle using CGPath
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

// Draw text using CoreText
static void cgtext(CGContextRef ctx, CGFloat x, CGFloat y, const char *str,
                   CTFontRef font, CGFloat r, CGFloat g, CGFloat b) {
    if (!str || !font) return;

    CFStringRef cfstr = CFStringCreateWithCString(nullptr, str, kCFStringEncodingUTF8);
    if (!cfstr) return;

    CGColorRef color = CGColorCreateGenericRGB(r, g, b, 1.0);
    CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef values[] = { font, color };
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr,
        (const void **)keys, (const void **)values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFAttributedStringRef astr = CFAttributedStringCreate(nullptr, cfstr, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(astr);

    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetTextPosition(ctx, x, y);
    CTLineDraw(line, ctx);

    CFRelease(line);
    CFRelease(astr);
    CFRelease(attrs);
    CGColorRelease(color);
    CFRelease(cfstr);
}

static CGFloat ct_text_width(const char *str, CTFontRef font) {
    if (!str || !font) return 0;
    CFStringRef cfstr = CFStringCreateWithCString(nullptr, str, kCFStringEncodingUTF8);
    if (!cfstr) return 0;
    CFStringRef keys[] = { kCTFontAttributeName };
    CFTypeRef values[] = { font };
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr,
        (const void **)keys, (const void **)values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef astr = CFAttributedStringCreate(nullptr, cfstr, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(astr);
    CGFloat w = (CGFloat)CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    CFRelease(line);
    CFRelease(astr);
    CFRelease(attrs);
    CFRelease(cfstr);
    return w;
}

// ---------------------------------------------------------------------------
// SilvertuneView
// ---------------------------------------------------------------------------

@interface SilvertuneView : NSView {
    SilvertunePlugin *_plugin;
    NSTimer          *_timer;
    CTFontRef         _fontLabel;   // ~10pt
    CTFontRef         _fontValue;   // ~12pt
    CTFontRef         _fontNote;    // ~18pt
}
- (instancetype)initWithPlugin:(SilvertunePlugin *)plugin;
@end

@implementation SilvertuneView

- (instancetype)initWithPlugin:(SilvertunePlugin *)plugin {
    self = [super initWithFrame:NSMakeRect(0, 0, GUI_W, GUI_H)];
    if (self) {
        _plugin = plugin;
        // Use Menlo first, fall back to Courier New
        _fontLabel = CTFontCreateWithName(CFSTR("Menlo"), 10.0, nullptr);
        if (!_fontLabel) _fontLabel = CTFontCreateWithName(CFSTR("Courier New"), 10.0, nullptr);
        _fontValue = CTFontCreateWithName(CFSTR("Menlo"), 12.0, nullptr);
        if (!_fontValue) _fontValue = CTFontCreateWithName(CFSTR("Courier New"), 12.0, nullptr);
        _fontNote  = CTFontCreateWithName(CFSTR("Menlo-Bold"), 18.0, nullptr);
        if (!_fontNote)  _fontNote  = CTFontCreateWithName(CFSTR("Courier New Bold"), 18.0, nullptr);

        _timer = [NSTimer scheduledTimerWithTimeInterval:1.0/30.0
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
    if (_fontLabel) { CFRelease(_fontLabel); _fontLabel = nullptr; }
    if (_fontValue) { CFRelease(_fontValue); _fontValue = nullptr; }
    if (_fontNote)  { CFRelease(_fontNote);  _fontNote  = nullptr; }
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

    // --- Left display panel (OLED arc needle meter) ---
    cgrect(ctx, DISP_X, DISP_Y, DISP_W, DISP_H,
           R(COL_DIM_GREEN), G(COL_DIM_GREEN), B(COL_DIM_GREEN));
    cgfill(ctx, DISP_X + 1, DISP_Y + 1, DISP_W - 2, DISP_H - 2,
           R(COL_DISPLAY_BG), G(COL_DISPLAY_BG), B(COL_DISPLAY_BG));

    const CGFloat pcx   = DISP_X + DISP_W / 2.0;
    const CGFloat pcy   = DISP_Y + DISP_H - 60.0;
    const CGFloat arc_r = 54.0;

    CGFloat font_label_ascent = CTFontGetAscent(_fontLabel);
    CGFloat font_label_height = CTFontGetAscent(_fontLabel) + CTFontGetDescent(_fontLabel) + CTFontGetLeading(_fontLabel);
    CGFloat font_value_ascent = CTFontGetAscent(_fontValue);
    CGFloat font_value_height = CTFontGetAscent(_fontValue) + CTFontGetDescent(_fontValue);

    // Title centered
    {
        CGFloat tw = ct_text_width("SILVERTUNE", _fontLabel);
        cgtext(ctx, pcx - tw / 2.0, DISP_Y + 5 + font_label_ascent, "SILVERTUNE",
               _fontLabel, R(COL_GREEN), G(COL_GREEN), B(COL_GREEN));
    }

    // Separator
    CGFloat sep_y = DISP_Y + 5 + font_label_height + 3;
    cgline(ctx, DISP_X + 2, sep_y, DISP_X + DISP_W - 3, sep_y,
           R(COL_DIM_GREEN), G(COL_DIM_GREEN), B(COL_DIM_GREEN));

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
            CGFloat x = PIANO_KX + i * PIANO_WK_W;
            bool lit  = (WK[i] == hi);
            if (lit)
                cgfill(ctx, x, PIANO_KY, PIANO_WK_W - 1, PIANO_WK_H,
                       R(COL_GREEN), G(COL_GREEN), B(COL_GREEN));
            else
                cgrect(ctx, x, PIANO_KY, PIANO_WK_W, PIANO_WK_H + 1,
                       R(COL_DIM_GREEN), G(COL_DIM_GREEN), B(COL_DIM_GREEN));
        }
        for (int i = 0; i < 5; ++i) {
            CGFloat x = PIANO_KX + BK[i].dx;
            bool lit  = (BK[i].note == hi);
            if (lit)
                cgfill(ctx, x, PIANO_KY, PIANO_BK_W, PIANO_BK_H,
                       R(COL_GREEN), G(COL_GREEN), B(COL_GREEN));
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

    // Arc: upper semicircle as polyline (smooth with CG anti-aliasing)
    CGContextSetRGBStrokeColor(ctx, R(COL_DIM_GREEN), G(COL_DIM_GREEN), B(COL_DIM_GREEN), 1.0);
    CGContextSetLineWidth(ctx, 1.0);
    CGContextBeginPath(ctx);
    for (int i = 0; i <= 48; ++i) {
        double a = (double)i * M_PI / 48.0;
        CGFloat x = pcx + (CGFloat)(arc_r * std::cos(a));
        CGFloat y = pcy - (CGFloat)(arc_r * std::sin(a));
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
            CGFloat ix = pcx + (CGFloat)((arc_r - tk.len) * ca);
            CGFloat iy = pcy - (CGFloat)((arc_r - tk.len) * sa);
            CGFloat ox = pcx + (CGFloat)((arc_r + 3)      * ca);
            CGFloat oy = pcy - (CGFloat)((arc_r + 3)      * sa);
            if (tk.cv == 0)
                cgline(ctx, ix, iy, ox, oy, R(COL_DIM_GREEN), G(COL_DIM_GREEN), B(COL_DIM_GREEN));
            else
                cgline(ctx, ix, iy, ox, oy, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        }
    }

    // Needle from pivot to arc, smoothed
    {
        double a   = (90.0 - (double)dc * 90.0 / 50.0) * M_PI / 180.0;
        CGFloat nx = pcx + (CGFloat)((arc_r - 5) * std::cos(a));
        CGFloat ny = pcy - (CGFloat)((arc_r - 5) * std::sin(a));
        bool in_tune = std::fabs(dc) < 5.0f;
        if (!active)
            cgline(ctx, pcx, pcy, nx, ny, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        else if (in_tune)
            cgline(ctx, pcx, pcy, nx, ny, R(COL_GREEN), G(COL_GREEN), B(COL_GREEN));
        else
            cgline(ctx, pcx, pcy, nx, ny, 1.0, 1.0, 1.0);

        CGFloat nr = active ? (in_tune ? R(COL_GREEN) : 1.0) : R(COL_LABEL);
        CGFloat ng = active ? (in_tune ? G(COL_GREEN) : 1.0) : G(COL_LABEL);
        CGFloat nb = active ? (in_tune ? B(COL_GREEN) : 1.0) : B(COL_LABEL);
        cgcircle(ctx, nx,  ny,  2.0, nr, ng, nb);
        cgcircle(ctx, pcx, pcy, 2.0, R(COL_DIM_GREEN), G(COL_DIM_GREEN), B(COL_DIM_GREEN));
    }

    // CORR note below pivot (centered)
    {
        const char *corr_str = (corr >= 0) ? NOTE_NAMES[corr % 12] : "--";
        CGFloat nw = ct_text_width(corr_str, _fontValue);
        cgtext(ctx, pcx - nw / 2.0, pcy + 14 + font_value_ascent, corr_str,
               _fontValue, 1.0, 1.0, 1.0);
    }

    // Cents value below CORR note
    if (active) {
        float raw = (dmidi - (float)corr) * 100.0f;
        char cbuf[16];
        snprintf(cbuf, sizeof(cbuf), "%+.0fc", raw);
        CGFloat cw = ct_text_width(cbuf, _fontLabel);
        bool in_tune = std::fabs(dc) < 5.0f;
        cgtext(ctx, pcx - cw / 2.0,
               pcy + 14 + font_value_height + 4 + font_label_ascent,
               cbuf, _fontLabel,
               in_tune ? R(COL_GREEN) : R(COL_LABEL),
               in_tune ? G(COL_GREEN) : G(COL_LABEL),
               in_tune ? B(COL_GREEN) : B(COL_LABEL));
    }

    // --- KEY stepper ---
    cgtext(ctx, KEY_LABEL_X, KEY_LABEL_Y + font_label_ascent, "KEY",
           _fontLabel, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
    cg_left_triangle(ctx, KEY_LEFT_X, KEY_BTN_Y,
                     R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
    cg_right_triangle(ctx, KEY_RIGHT_X, KEY_BTN_Y,
                      R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
    {
        int key = (int)std::lround(p->param_key.load());
        key = ((key % 12) + 12) % 12;
        cgtext(ctx, KEY_TEXT_X, KEY_BTN_Y + font_value_ascent, NOTE_NAMES[key],
               _fontValue, R(COL_GREEN), G(COL_GREEN), B(COL_GREEN));
    }

    // --- SCALE stepper ---
    cgtext(ctx, SCALE_LABEL_X, SCALE_LABEL_Y + font_label_ascent, "SCALE",
           _fontLabel, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
    cg_left_triangle(ctx, SCALE_LEFT_X, SCALE_BTN_Y,
                     R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
    cg_right_triangle(ctx, SCALE_RIGHT_X, SCALE_BTN_Y,
                      R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
    {
        int ps = (int)std::lround(p->param_scale.load());
        ps = ps < 0 ? 0 : (ps > 2 ? 2 : ps);
        int gs = PARAM_TO_GUI_SCALE[ps];
        cgtext(ctx, SCALE_TEXT_X, SCALE_BTN_Y + font_value_ascent, SCALE_NAMES_GUI[gs],
               _fontValue, R(COL_GREEN), G(COL_GREEN), B(COL_GREEN));
    }

    // --- WIDE slider ---
    {
        float wide = (float)p->param_wide.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", wide * 100.0f);
        cgtext(ctx, WIDE_LABEL_X, WIDE_LABEL_Y + font_label_ascent, "WIDE",
               _fontLabel, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        // Right-align percent
        CGFloat pw = ct_text_width(pct, _fontValue);
        cgtext(ctx, WIDE_PCT_X - pw, WIDE_PCT_Y + font_value_ascent, pct,
               _fontValue, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        // Track
        cgfill(ctx, WIDE_TRACK_X, WIDE_TRACK_Y, WIDE_TRACK_W, WIDE_TRACK_H,
               R(COL_TRACK), G(COL_TRACK), B(COL_TRACK));
        int fw = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W) - WIDE_TRACK_X;
        if (fw > 0)
            cgfill(ctx, WIDE_TRACK_X, WIDE_TRACK_Y, fw, WIDE_TRACK_H,
                   R(COL_FILL), G(COL_FILL), B(COL_FILL));
        int tx = slider_px(wide, WIDE_TRACK_X, WIDE_TRACK_W);
        int ty = WIDE_TRACK_Y + WIDE_TRACK_H / 2;
        cgcircle(ctx, tx, ty, SLIDER_THUMB_R,
                 R(COL_THUMB), G(COL_THUMB), B(COL_THUMB));
    }

    // --- TUNE slider ---
    {
        float tune = (float)p->param_speed.load();
        char pct[16];
        snprintf(pct, sizeof(pct), "%.0f%%", tune * 100.0f);
        cgtext(ctx, TUNE_LABEL_X, TUNE_LABEL_Y + font_label_ascent, "TUNE",
               _fontLabel, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        CGFloat pw = ct_text_width(pct, _fontValue);
        cgtext(ctx, TUNE_PCT_X - pw, TUNE_PCT_Y + font_value_ascent, pct,
               _fontValue, R(COL_LABEL), G(COL_LABEL), B(COL_LABEL));
        cgfill(ctx, TUNE_TRACK_X, TUNE_TRACK_Y, TUNE_TRACK_W, TUNE_TRACK_H,
               R(COL_TRACK), G(COL_TRACK), B(COL_TRACK));
        int fw = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W) - TUNE_TRACK_X;
        if (fw > 0)
            cgfill(ctx, TUNE_TRACK_X, TUNE_TRACK_Y, fw, TUNE_TRACK_H,
                   R(COL_FILL), G(COL_FILL), B(COL_FILL));
        int tx = slider_px(tune, TUNE_TRACK_X, TUNE_TRACK_W);
        int ty = TUNE_TRACK_Y + TUNE_TRACK_H / 2;
        cgcircle(ctx, tx, ty, SLIDER_THUMB_R,
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
        // Stop timer via dealloc; remove from superview
        [view removeFromSuperview];
        view = nil;
    }
}

#endif // __APPLE__
