#pragma once
#include <cstdint>

// The panel expects RGB565 with the high byte first, so every colour stored in the
// framebuffer is byte-swapped. These helpers convert to and from that form.

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
}

constexpr uint16_t swapEndian16(uint16_t val) {
    return static_cast<uint16_t>((val >> 8) | (val << 8));
}

// 0xRRGGBB -> panel colour
constexpr uint16_t colorHex(uint32_t rgb) {
    return swapEndian16(rgb565((rgb >> 19) & 0x1F, (rgb >> 10) & 0x3F, (rgb >> 3) & 0x1F));
}

// 0xRRGGBB mix: percent of fg over bg, still 0xRRGGBB
constexpr uint32_t mixHex(uint32_t fg, uint32_t bg, int percent) {
    return ((((fg >> 16) & 0xFF) * percent + ((bg >> 16) & 0xFF) * (100 - percent)) / 100) << 16 |
           ((((fg >> 8) & 0xFF) * percent + ((bg >> 8) & 0xFF) * (100 - percent)) / 100) << 8 |
           (((fg & 0xFF) * percent + (bg & 0xFF) * (100 - percent)) / 100);
}

// Mix fg over bg. Both are in panel byte order; alpha is 0 (bg) to 15 (fg).
inline uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
    if (alpha >= 15) return fg;
    if (alpha == 0) return bg;
    uint16_t f = swapEndian16(fg), b = swapEndian16(bg);
    int inv = 15 - alpha;
    int r = (((f >> 11) & 0x1F) * alpha + ((b >> 11) & 0x1F) * inv + 7) / 15;
    int g = (((f >> 5) & 0x3F) * alpha + ((b >> 5) & 0x3F) * inv + 7) / 15;
    int bl = ((f & 0x1F) * alpha + (b & 0x1F) * inv + 7) / 15;
    return swapEndian16(static_cast<uint16_t>((r << 11) | (g << 5) | bl));
}
