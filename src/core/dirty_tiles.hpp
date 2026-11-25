#pragma once
#include <cstdint>
#include <vector>

// Finds what changed between two frames and covers it with rectangles small enough
// for one panel update command each.
struct Tile {
    int x, y, w, h;
};

namespace tiles {
    constexpr int MAX_PIXELS = 2048;   // 4096 payload bytes, 2 bytes per pixel
    constexpr int MAX_WIDTH = 254;     // width travels in one byte
    constexpr int BAND_HEIGHT = 12;    // taller than a line of small text
}

// prev may be null, meaning nothing is known about the panel and everything is dirty.
// Tiles come back top to bottom, so a page change paints downwards.
std::vector<Tile> dirtyTiles(const uint16_t* prev, const uint16_t* cur, int width, int height);
