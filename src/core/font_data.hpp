#pragma once
#include <cstdint>

// Pre-rendered anti-aliased glyphs (see tools/bake_fonts.py).
struct Glyph {
    uint16_t offset;   // into FontData::alpha
    uint8_t w, h;      // bitmap size
    int8_t xoff;       // from the pen position
    int8_t yoff;       // from the top of the capital letters
    uint8_t advance;
};

struct FontData {
    const Glyph* glyphs;
    const uint8_t* alpha;   // 4 bits per pixel, high nibble first
    uint8_t first;          // first character code
    uint8_t count;
    uint8_t capHeight;
    uint8_t digitAdvance;   // widest digit, used for fixed-width numbers
};

extern const FontData FONT_SMALL;
extern const FontData FONT_TITLE;
extern const FontData FONT_LARGE;
