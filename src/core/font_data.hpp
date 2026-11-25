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

// The fonts for one display scale, indexed by the Font enum
struct FontSet {
    int scalePercent;
    const FontData* fonts[3];
};

// The baked set closest to the requested scale (100 = one device pixel per layout unit)
const FontSet& fontSetFor(int scalePercent);
