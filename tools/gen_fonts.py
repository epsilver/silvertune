#!/usr/bin/env python3
"""
Generate font assets for Silvertune.

Outputs (all committed to repo — no build-time dependency on this script):
  src/inter_black.cpp   — Inter-Black TTF bytes as a C array (Win32 / Cocoa)
  include/inter_fonts.h — extern declarations for the above
  include/font6x8.h     — 6×8 pixel font rasterised from Inter-Black (X11)

Run from repo root:  python3 tools/gen_fonts.py
"""

from pathlib import Path

REPO     = Path(__file__).parent.parent
FONT_TTF = REPO / "assets/fonts/Inter-Black.ttf"

# ── Part 1: embed TTF as a C byte-array ────────────────────────────────────

def gen_embed():
    data     = FONT_TTF.read_bytes()
    cpp_path = REPO / "src/inter_black.cpp"
    h_path   = REPO / "include/inter_fonts.h"

    with open(cpp_path, "w") as f:
        f.write("// Auto-generated — do not edit.  Run tools/gen_fonts.py to regenerate.\n")
        f.write('#include "inter_fonts.h"\n\n')
        f.write("const unsigned char INTER_BLACK_FONT[] = {\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            f.write("    " + ", ".join(f"0x{b:02X}" for b in chunk))
            f.write(",\n" if i + 16 < len(data) else "\n")
        f.write("};\n\n")
        f.write(f"const unsigned int INTER_BLACK_FONT_LEN = {len(data)};\n")

    with open(h_path, "w") as f:
        f.write("// Auto-generated — do not edit.\n")
        f.write("#pragma once\n#include <cstdint>\n\n")
        f.write("extern const unsigned char INTER_BLACK_FONT[];\n")
        f.write("extern const unsigned int  INTER_BLACK_FONT_LEN;\n")

    print(f"  {cpp_path.name}  ({len(data):,} bytes → {len(data)//1024}K)")
    print(f"  {h_path.name}")

# ── Part 2: rasterise Inter-Black into a 6×8 pixel font ───────────────────

def render_glyph(font, ch, cell_w=6, cell_h=8, threshold=96):
    from PIL import Image, ImageDraw
    canvas = 48
    img = Image.new("L", (canvas, canvas), 0)
    ImageDraw.Draw(img).text((8, 8), ch, fill=255, font=font)

    nonzero = [(x % canvas, x // canvas)
               for x, p in enumerate(img.getdata()) if p > threshold]
    if not nonzero:
        return [0] * cell_h

    xs = [p[0] for p in nonzero]; ys = [p[1] for p in nonzero]
    gx0, gx1 = min(xs), max(xs)
    gy0, gy1 = min(ys), max(ys)
    gw = gx1 - gx0 + 1
    gh = gy1 - gy0 + 1

    # Centre horizontally; sit baseline at row cell_h-2
    ox = (cell_w - gw) // 2 - gx0
    oy = (cell_h - 2) - gh + 1 - gy0

    rows = []
    for row in range(cell_h):
        byte = 0
        sy = row - oy
        if 0 <= sy < canvas:
            for col in range(cell_w):
                sx = col - ox
                if 0 <= sx < canvas and img.getpixel((sx, sy)) > threshold:
                    byte |= (0x20 >> col)   # bit5 = leftmost pixel
        rows.append(byte)
    return rows

def gen_pixel_font():
    from PIL import ImageFont
    font    = ImageFont.truetype(str(FONT_TTF), 7)
    out     = REPO / "include/font6x8.h"

    with open(out, "w") as f:
        f.write("// Auto-generated pixel font — Inter-Black rasterised at 7px into 6×8 cells.\n")
        f.write("// Encoding per row byte: bit5=leftmost pixel … bit0=rightmost pixel.\n")
        f.write("// Covers printable ASCII 32–126 (95 glyphs, index = codepoint − 32).\n")
        f.write("// Run tools/gen_fonts.py to regenerate.\n")
        f.write("#pragma once\n#include <cstdint>\n\n")
        f.write("static constexpr int FONT6X8_W   = 6;  // pixels per glyph\n")
        f.write("static constexpr int FONT6X8_H   = 8;\n")
        f.write("static constexpr int FONT6X8_ADV = 7;  // advance (glyph + 1px gap)\n\n")
        f.write("static const uint8_t FONT6X8[95][8] = {\n")

        for cp in range(32, 127):
            rows = render_glyph(font, chr(cp))
            hex_bytes = ", ".join(f"0x{b:02X}" for b in rows)
            label = repr(chr(cp)) if chr(cp) not in ("\\", "'", "\x7f") else f"0x{cp:02X}"
            f.write(f"    {{{hex_bytes}}},  // {label}\n")

        f.write("};\n\n")
        f.write("// Pixel width of a string at the given integer scale factor.\n")
        f.write("static inline int font6x8_width(const char *s, int scale) {\n")
        f.write("    int n = 0; while (*s++) ++n;\n")
        f.write("    return n > 0 ? (n * FONT6X8_ADV - 1) * scale : 0;\n")
        f.write("}\n")

    print(f"  {out.name}  (95 glyphs, 6×8)")

# ── Entry point ────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("Generating font assets...")
    gen_embed()
    gen_pixel_font()
    print("Done.")
