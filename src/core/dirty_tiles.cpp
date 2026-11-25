#include "core/dirty_tiles.hpp"
#include <algorithm>

static bool rowDirty(const uint16_t* prev, const uint16_t* cur, int width, int y,
                     int& minX, int& maxX) {
    const uint16_t* c = cur + y * width;
    if (!prev) {
        minX = 0;
        maxX = width - 1;
        return true;
    }
    const uint16_t* p = prev + y * width;
    int first = 0;
    while (first < width && p[first] == c[first]) first++;
    if (first == width) return false;
    int last = width - 1;
    while (p[last] == c[last]) last--;
    minX = std::min(minX, first);
    maxX = std::max(maxX, last);
    return true;
}

std::vector<Tile> dirtyTiles(const uint16_t* prev, const uint16_t* cur, int width, int height) {
    std::vector<Tile> out;
    int y = 0;
    while (y < height) {
        // A band starts at the next changed row, so a line of text lands in one band
        int minX = width, maxX = -1;
        if (!rowDirty(prev, cur, width, y, minX, maxX)) {
            y++;
            continue;
        }
        int bandH = std::min(tiles::BAND_HEIGHT, height - y);
        int lastDirty = y;
        for (int r = y + 1; r < y + bandH; r++) {
            if (rowDirty(prev, cur, width, r, minX, maxX)) lastDirty = r;
        }
        int h = lastDirty - y + 1;
        
        // Split the changed span into equal pieces that each fit one command
        int span = maxX - minX + 1;
        int maxW = std::min(tiles::MAX_WIDTH, tiles::MAX_PIXELS / h);
        int pieces = (span + maxW - 1) / maxW;
        int pieceW = (span + pieces - 1) / pieces;
        for (int x = minX; x <= maxX; x += pieceW) {
            out.push_back({x, y, std::min(pieceW, maxX - x + 1), h});
        }
        y += h;
    }
    return out;
}
