#pragma once
#include <cstdint>

// Opaque font handle
struct StbFont;

// Init all sizes once — safe to call multiple times (noop after first)
void stb_fonts_init();

// Font size accessors
StbFont *font_sm();   // 11px — subtitle, cents
StbFont *font_md();   // 14px — labels, values
StbFont *font_lg();   // 26px — display note name

// Pixel height of text rendered with this font
int stb_font_height(StbFont *f);

// Width of string in pixels
int stb_font_width(StbFont *f, const char *str);

// Render callback: (px, py, alpha 0-255, userdata)
typedef void (*StbPixelFn)(int x, int y, uint8_t alpha, void *ud);

// Draw string — (x, y) = top-left corner of text
void stb_font_draw(StbFont *f, int x, int y, const char *str,
                   StbPixelFn fn, void *ud);
