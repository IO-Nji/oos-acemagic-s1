#include "core/framebuffer.hpp"
#include "core/font_data.hpp"
#include <algorithm>
#include <cmath>

Framebuffer::Framebuffer(int width, int height)
    : width_(width), height_(height), pixels_(width * height, 0) {}

void Framebuffer::clear(uint16_t color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Framebuffer::setPixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        pixels_[y * width_ + x] = color;
    }
}

void Framebuffer::blendPixel(int x, int y, uint16_t color, uint8_t alpha) {
    if (alpha == 0 || x < 0 || x >= width_ || y < 0 || y >= height_) return;
    uint16_t& dst = pixels_[y * width_ + x];
    dst = blend565(color, dst, alpha);
}

void Framebuffer::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    // Bresenham's line algorithm
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    
    while (true) {
        setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Framebuffer::drawRect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    drawHLine(x, y, w, color);
    drawHLine(x, y + h - 1, w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w - 1, y, h, color);
}

void Framebuffer::fillRect(int x, int y, int w, int h, uint16_t color) {
    int x0 = std::max(x, 0), y0 = std::max(y, 0);
    int x1 = std::min(x + w, width_), y1 = std::min(y + h, height_);
    if (x0 >= x1) return;
    for (int py = y0; py < y1; py++) {
        std::fill_n(pixels_.begin() + py * width_ + x0, x1 - x0, color);
    }
}

void Framebuffer::fillCircle(int cx, int cy, int r, uint16_t color) {
    for (int y = -r - 1; y <= r + 1; y++) {
        for (int x = -r - 1; x <= r + 1; x++) {
            float cover = r + 0.5f - std::sqrt(static_cast<float>(x * x + y * y));
            if (cover <= 0.0f) continue;
            blendPixel(cx + x, cy + y, color, static_cast<uint8_t>(std::min(1.0f, cover) * 15.0f + 0.5f));
        }
    }
}

void Framebuffer::drawHLine(int x, int y, int w, uint16_t color) {
    fillRect(x, y, w, 1, color);
}

void Framebuffer::drawVLine(int x, int y, int h, uint16_t color) {
    fillRect(x, y, 1, h, color);
}

void Framebuffer::drawRoundedRect(int x, int y, int w, int h, int radius, uint16_t color) {
    if (radius <= 0) {
        drawRect(x, y, w, h, color);
        return;
    }
    
    radius = std::min(radius, std::min(w / 2, h / 2));
    
    drawHLine(x + radius, y, w - 2 * radius, color);
    drawHLine(x + radius, y + h - 1, w - 2 * radius, color);
    drawVLine(x, y + radius, h - 2 * radius, color);
    drawVLine(x + w - 1, y + radius, h - 2 * radius, color);
    
    // Corner arcs (midpoint circle)
    int rx = radius, ry = 0;
    int err = 0;
    
    while (rx >= ry) {
        setPixel(x + w - 1 - radius + rx, y + radius - ry, color);
        setPixel(x + w - 1 - radius + ry, y + radius - rx, color);
        setPixel(x + radius - rx, y + radius - ry, color);
        setPixel(x + radius - ry, y + radius - rx, color);
        setPixel(x + w - 1 - radius + rx, y + h - 1 - radius + ry, color);
        setPixel(x + w - 1 - radius + ry, y + h - 1 - radius + rx, color);
        setPixel(x + radius - rx, y + h - 1 - radius + ry, color);
        setPixel(x + radius - ry, y + h - 1 - radius + rx, color);
        
        ry++;
        err += 1 + 2 * ry;
        if (2 * (err - rx) + 1 > 0) {
            rx--;
            err += 1 - 2 * rx;
        }
    }
}

void Framebuffer::fillRoundedRect(int x, int y, int w, int h, int radius, uint16_t color) {
    if (radius <= 0) {
        fillRect(x, y, w, h, color);
        return;
    }
    
    radius = std::min(radius, std::min(w / 2, h / 2));
    
    fillRect(x + radius, y, w - 2 * radius, h, color);
    fillRect(x, y + radius, radius, h - 2 * radius, color);
    fillRect(x + w - radius, y + radius, radius, h - 2 * radius, color);
    
    for (int cy = 0; cy < radius; cy++) {
        for (int cx = 0; cx < radius; cx++) {
            int dx = radius - cx - 1;
            int dy = radius - cy - 1;
            if (dx * dx + dy * dy <= radius * radius) {
                setPixel(x + cx, y + cy, color);
                setPixel(x + w - 1 - cx, y + cy, color);
                setPixel(x + cx, y + h - 1 - cy, color);
                setPixel(x + w - 1 - cx, y + h - 1 - cy, color);
            }
        }
    }
}

void Framebuffer::drawSegmentedBar(int x, int y, int w, int h, float percent,
                                    uint16_t fgColor, uint16_t bgColor, int segments, int gap) {
    percent = std::max(0.0f, std::min(100.0f, percent));
    
    int segW = (w - (segments - 1) * gap) / segments;
    int litSegments = static_cast<int>(segments * percent / 100.0f + 0.5f);
    
    for (int i = 0; i < segments; i++) {
        int sx = x + i * (segW + gap);
        uint16_t color = (i < litSegments) ? fgColor : bgColor;
        fillRect(sx, y, segW, h, color);
    }
}

void Framebuffer::drawRingGauge(int cx, int cy, int outerR, int innerR, float percent,
                                 uint16_t fgColor, uint16_t bgColor) {
    percent = std::max(0.0f, std::min(100.0f, percent));
    const float fraction = percent / 100.0f;
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    
    for (int y = -outerR - 1; y <= outerR + 1; y++) {
        for (int x = -outerR - 1; x <= outerR + 1; x++) {
            float d = std::sqrt(static_cast<float>(x * x + y * y));
            float cover = std::min(outerR + 0.5f - d, d - (innerR - 0.5f));
            if (cover <= 0.0f) continue;
            
            // Angle clockwise from 12 o'clock, 0..1
            float a = std::atan2(static_cast<float>(x), static_cast<float>(-y));
            if (a < 0.0f) a += twoPi;
            bool lit = fraction >= 1.0f || (fraction > 0.0f && a / twoPi <= fraction);
            
            blendPixel(cx + x, cy + y, lit ? fgColor : bgColor,
                       static_cast<uint8_t>(std::min(1.0f, cover) * 15.0f + 0.5f));
        }
    }
}

void Framebuffer::drawStackedBar(int x, int y, int w, int h,
                                  const float* percents, const uint16_t* colors, int count,
                                  uint16_t borderColor) {
    drawRect(x, y, w, h, borderColor);
    
    int currentX = x + 1;
    int barW = w - 2;
    int barH = h - 2;
    
    for (int i = 0; i < count; i++) {
        int segmentW = static_cast<int>((percents[i] / 100.0f) * barW);
        if (segmentW > 0 && currentX < x + w - 1) {
            int actualW = std::min(segmentW, x + w - 1 - currentX);
            fillRect(currentX, y + 1, actualW, barH, colors[i]);
            currentX += actualW;
        }
    }
}

void Framebuffer::drawSparkline(int x, int y, int w, int h, const float* values, int count,
                                 uint16_t lineColor, bool filled, uint16_t fillColor) {
    if (count < 2) return;
    
    float minVal = values[0], maxVal = values[0];
    for (int i = 1; i < count; i++) {
        minVal = std::min(minVal, values[i]);
        maxVal = std::max(maxVal, values[i]);
    }
    
    if (maxVal - minVal < 0.001f) {
        maxVal = minVal + 1.0f;
    }
    
    float xStep = static_cast<float>(w) / (count - 1);
    
    if (filled && fillColor != 0) {
        for (int i = 0; i < count - 1; i++) {
            int x1 = x + static_cast<int>(i * xStep);
            int x2 = x + static_cast<int>((i + 1) * xStep);
            int y1 = y + h - static_cast<int>((values[i] - minVal) / (maxVal - minVal) * h);
            int y2 = y + h - static_cast<int>((values[i + 1] - minVal) / (maxVal - minVal) * h);
            
            for (int px = x1; px <= x2; px++) {
                float t = (px - x1) / (float)(x2 - x1 + 1);
                int py = y1 + static_cast<int>(t * (y2 - y1));
                drawVLine(px, py, y + h - py, fillColor);
            }
        }
    }
    
    for (int i = 0; i < count - 1; i++) {
        int x1 = x + static_cast<int>(i * xStep);
        int x2 = x + static_cast<int>((i + 1) * xStep);
        int y1 = y + h - static_cast<int>((values[i] - minVal) / (maxVal - minVal) * h);
        int y2 = y + h - static_cast<int>((values[i + 1] - minVal) / (maxVal - minVal) * h);
        drawLine(x1, y1, x2, y2, lineColor);
    }
}

static const FontData& fontData(Font font) {
    switch (font) {
        case Font::Title: return FONT_TITLE;
        case Font::Large: return FONT_LARGE;
        default:          return FONT_SMALL;
    }
}

static const Glyph& glyphFor(const FontData& fd, char c) {
    unsigned char u = static_cast<unsigned char>(c);
    if (u < fd.first || u >= fd.first + fd.count) u = '?';
    return fd.glyphs[u - fd.first];
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

void Framebuffer::drawText(int x, int y, const std::string& str, uint16_t color, Font font) {
    const FontData& fd = fontData(font);
    int penX = x;
    for (char c : str) {
        const Glyph& g = glyphFor(fd, c);
        const uint8_t* src = fd.alpha + g.offset;
        for (int row = 0; row < g.h; row++) {
            for (int col = 0; col < g.w; col++) {
                int i = row * g.w + col;
                uint8_t a = (i & 1) ? (src[i >> 1] & 0x0F) : (src[i >> 1] >> 4);
                blendPixel(penX + g.xoff + col, y + g.yoff + row, color, a);
            }
        }
        penX += g.advance;
    }
    if (audit_ && !str.empty()) {
        textBoxes_.push_back({x, y, penX - x, fd.capHeight, str});
    }
}

void Framebuffer::drawTextRight(int xRight, int y, const std::string& str, uint16_t color,
                                Font font) {
    drawText(xRight - textWidth(str, font), y, str, color, font);
}

void Framebuffer::drawTextCentered(int cx, int y, const std::string& str, uint16_t color,
                                   Font font) {
    drawText(cx - textWidth(str, font) / 2, y, str, color, font);
}

void Framebuffer::drawTextSteady(int xRight, int y, const std::string& str, uint16_t color,
                                 Font font) {
    drawText(xRight - maxNumericWidth(str, font), y, str, color, font);
}

int Framebuffer::textWidth(const std::string& str, Font font) const {
    const FontData& fd = fontData(font);
    int w = 0;
    for (char c : str) w += glyphFor(fd, c).advance;
    return w;
}

int Framebuffer::maxNumericWidth(const std::string& str, Font font) const {
    const FontData& fd = fontData(font);
    int w = 0;
    for (char c : str) w += isDigit(c) ? fd.digitAdvance : glyphFor(fd, c).advance;
    return w;
}

int Framebuffer::textHeight(Font font) const {
    return fontData(font).capHeight;
}

std::string Framebuffer::fitText(const std::string& str, int maxWidth, Font font) const {
    if (textWidth(str, font) <= maxWidth) return str;
    std::string s = str;
    while (!s.empty() && textWidth(s + "..", font) > maxWidth) {
        s.pop_back();
    }
    return s.empty() ? s : s + "..";
}
