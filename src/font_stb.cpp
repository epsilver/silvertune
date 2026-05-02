#define STB_TRUETYPE_IMPLEMENTATION
#include "../libs/stb_truetype.h"
#include "font_stb.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

extern const unsigned char STB_FONT_DATA[];
extern const unsigned int  STB_FONT_LEN;

struct StbFont {
    stbtt_fontinfo info;
    float          scale;
    int            ascent;
    int            height;   // ascent - descent (pixel cell height)
};

static StbFont *s_sm  = nullptr;
static StbFont *s_md  = nullptr;
static StbFont *s_lg  = nullptr;

static StbFont *make_font(float px) {
    StbFont *f = new StbFont();
    if (!stbtt_InitFont(&f->info, STB_FONT_DATA, 0)) { delete f; return nullptr; }
    f->scale = stbtt_ScaleForPixelHeight(&f->info, px);
    int a, d, g;
    stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
    f->ascent = (int)std::ceil(a * f->scale);
    f->height = (int)std::ceil((a - d) * f->scale);
    return f;
}

void stb_fonts_init() {
    if (s_sm) return;
    s_sm = make_font(11.0f);
    s_md = make_font(14.0f);
    s_lg = make_font(26.0f);
}

StbFont *font_sm() { return s_sm; }
StbFont *font_md() { return s_md; }
StbFont *font_lg() { return s_lg; }

int stb_font_height(StbFont *f) { return f ? f->height : 0; }

int stb_font_width(StbFont *f, const char *str) {
    if (!f || !str) return 0;
    int w = 0;
    for (const char *p = str; *p; ++p) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&f->info, (unsigned char)*p, &adv, &lsb);
        w += (int)(adv * f->scale + 0.5f);
        if (*(p + 1))
            w += (int)(stbtt_GetCodepointKernAdvance(&f->info, (unsigned char)*p,
                                                      (unsigned char)*(p+1)) * f->scale);
    }
    return w;
}

void stb_font_draw(StbFont *f, int x, int y, const char *str,
                   StbPixelFn fn, void *ud) {
    if (!f || !str) return;
    int cx = x;
    int baseline = y + f->ascent;
    for (const char *p = str; *p; ++p) {
        int w, h, xoff, yoff;
        uint8_t *bmp = stbtt_GetCodepointBitmap(
            &f->info, f->scale, f->scale, (unsigned char)*p, &w, &h, &xoff, &yoff);
        if (bmp) {
            for (int row = 0; row < h; ++row)
                for (int col = 0; col < w; ++col) {
                    uint8_t a = bmp[row * w + col];
                    if (a > 8)
                        fn(cx + xoff + col, baseline + yoff + row, a, ud);
                }
            stbtt_FreeBitmap(bmp, nullptr);
        }
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&f->info, (unsigned char)*p, &adv, &lsb);
        cx += (int)(adv * f->scale + 0.5f);
        if (*(p + 1))
            cx += (int)(stbtt_GetCodepointKernAdvance(&f->info, (unsigned char)*p,
                                                       (unsigned char)*(p+1)) * f->scale);
    }
}
