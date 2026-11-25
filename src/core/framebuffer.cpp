#include "core/framebuffer.hpp"
#include "core/font_data.hpp"
#include <algorithm>
#include <cmath>

Framebuffer::Framebuffer(int width, int height, UiScale scale)
    : width_(width), height_(height), scale_(scale),
      devWidth_(dev(width)), devHeight_(dev(height)),
      stroke_(std::max(1, scale.num / scale.den)),
      pixels_(static_cast<size_t>(devWidth_) * devHeight_, 0) {}

// Round to the nearest device pixel; exact at scale 1
int Framebuffer::dev(int v) const {
    if (scale_.num == scale_.den) return v;
    return static_cast<int>(std::floor(static_cast<double>(v) * scale_.num / scale_.den + 0.5));
}

void Framebuffer::clear(uint16_t color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Framebuffer::devPixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < devWidth_ && y >= 0 && y < devHeight_) {
        pixels_[y * devWidth_ + x] = color;
    }
}

void Framebuffer::devBlend(int x, int y, uint16_t color, uint8_t alpha) {
    if (alpha == 0 || x < 0 || x >= devWidth_ || y < 0 || y >= devHeight_) return;
    uint16_t& dst = pixels_[y * devWidth_ + x];
    dst = blend565(color, dst, alpha);
}

void Framebuffer::devFill(int x0, int y0, int x1, int y1, uint16_t color) {
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, devWidth_);
    y1 = std::min(y1, devHeight_);
    if (x0 >= x1) return;
    for (int y = y0; y < y1; y++) {
        std::fill_n(pixels_.begin() + y * devWidth_ + x0, x1 - x0, color);
    }
}

void Framebuffer::fillRect(int x, int y, int w, int h, uint16_t color) {
    devFill(dev(x), dev(y), dev(x + w), dev(y + h), color);
}

void Framebuffer::drawHLine(int x, int y, int w, uint16_t color) {
    devFill(dev(x), dev(y), dev(x + w), dev(y) + stroke_, color);
}

void Framebuffer::drawVLine(int x, int y, int h, uint16_t color) {
    devFill(dev(x), dev(y), dev(x) + stroke_, dev(y + h), color);
}

void Framebuffer::drawRect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int x0 = dev(x), y0 = dev(y), x1 = dev(x + w), y1 = dev(y + h);
    devFill(x0, y0, x1, y0 + stroke_, color);
    devFill(x0, y1 - stroke_, x1, y1, color);
    devFill(x0, y0, x0 + stroke_, y1, color);
    devFill(x1 - stroke_, y0, x1, y1, color);
}

void Framebuffer::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    // Bresenham's line algorithm, in device pixels
    x0 = dev(x0); y0 = dev(y0); x1 = dev(x1); y1 = dev(y1);
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    
    while (true) {
        devFill(x0, y0, x0 + stroke_, y0 + stroke_, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Framebuffer::fillCircle(int cx, int cy, int r, uint16_t color) {
    const int dcx = dev(cx), dcy = dev(cy);
    const float radius = devLength(r);
    const int reach = static_cast<int>(std::ceil(radius)) + 1;
    for (int y = -reach; y <= reach; y++) {
        for (int x = -reach; x <= reach; x++) {
            float cover = radius + 0.5f - std::sqrt(static_cast<float>(x * x + y * y));
            if (cover <= 0.0f) continue;
            devBlend(dcx + x, dcy + y, color, static_cast<uint8_t>(std::min(1.0f, cover) * 15.0f + 0.5f));
        }
    }
}

void Framebuffer::drawRoundedRect(int x, int y, int w, int h, int radius, uint16_t color) {
    if (radius <= 0) {
        drawRect(x, y, w, h, color);
        return;
    }
    
    // Device-pixel box and corner radius
    const int x0 = dev(x), y0 = dev(y);
    const int dw = dev(x + w) - x0, dh = dev(y + h) - y0;
    const int r = std::min(static_cast<int>(devLength(radius) + 0.5f), std::min(dw / 2, dh / 2));
    
    devFill(x0 + r, y0, x0 + dw - r, y0 + 1, color);
    devFill(x0 + r, y0 + dh - 1, x0 + dw - r, y0 + dh, color);
    devFill(x0, y0 + r, x0 + 1, y0 + dh - r, color);
    devFill(x0 + dw - 1, y0 + r, x0 + dw, y0 + dh - r, color);
    
    // Corner arcs (midpoint circle)
    int rx = r, ry = 0;
    int err = 0;
    
    while (rx >= ry) {
        devPixel(x0 + dw - 1 - r + rx, y0 + r - ry, color);
        devPixel(x0 + dw - 1 - r + ry, y0 + r - rx, color);
        devPixel(x0 + r - rx, y0 + r - ry, color);
        devPixel(x0 + r - ry, y0 + r - rx, color);
        devPixel(x0 + dw - 1 - r + rx, y0 + dh - 1 - r + ry, color);
        devPixel(x0 + dw - 1 - r + ry, y0 + dh - 1 - r + rx, color);
        devPixel(x0 + r - rx, y0 + dh - 1 - r + ry, color);
        devPixel(x0 + r - ry, y0 + dh - 1 - r + rx, color);
        
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
    
    const int x0 = dev(x), y0 = dev(y);
    const int dw = dev(x + w) - x0, dh = dev(y + h) - y0;
    const int r = std::min(static_cast<int>(devLength(radius) + 0.5f), std::min(dw / 2, dh / 2));
    
    devFill(x0 + r, y0, x0 + dw - r, y0 + dh, color);
    devFill(x0, y0 + r, x0 + r, y0 + dh - r, color);
    devFill(x0 + dw - r, y0 + r, x0 + dw, y0 + dh - r, color);
    
    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            int dx = r - cx - 1;
            int dy = r - cy - 1;
            if (dx * dx + dy * dy <= r * r) {
                devPixel(x0 + cx, y0 + cy, color);
                devPixel(x0 + dw - 1 - cx, y0 + cy, color);
                devPixel(x0 + cx, y0 + dh - 1 - cy, color);
                devPixel(x0 + dw - 1 - cx, y0 + dh - 1 - cy, color);
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
    const int dcx = dev(cx), dcy = dev(cy);
    const float outer = devLength(outerR), inner = devLength(innerR);
    const int reach = static_cast<int>(std::ceil(outer)) + 1;
    
    for (int y = -reach; y <= reach; y++) {
        for (int x = -reach; x <= reach; x++) {
            float d = std::sqrt(static_cast<float>(x * x + y * y));
            float cover = std::min(outer + 0.5f - d, d - (inner - 0.5f));
            if (cover <= 0.0f) continue;
            
            // Angle clockwise from 12 o'clock, 0..1
            float a = std::atan2(static_cast<float>(x), static_cast<float>(-y));
            if (a < 0.0f) a += twoPi;
            bool lit = fraction >= 1.0f || (fraction > 0.0f && a / twoPi <= fraction);
            
            devBlend(dcx + x, dcy + y, lit ? fgColor : bgColor,
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
                fillRect(px, py, 1, y + h - py, fillColor);
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

// --- text -------------------------------------------------------------------

static const Glyph& glyphFor(const FontData& fd, char c) {
    unsigned char u = static_cast<unsigned char>(c);
    if (u < fd.first || u >= fd.first + fd.count) u = '?';
    return fd.glyphs[u - fd.first];
}

static const FontData& fontData(UiScale scale, Font font) {
    return *fontSetFor(scale.num * 100 / scale.den).fonts[static_cast<int>(font)];
}

int Framebuffer::devTextWidth(const std::string& str, Font font, bool steadyDigits) const {
    const FontData& fd = fontData(scale_, font);
    int w = 0;
    for (char c : str) {
        bool digit = c >= '0' && c <= '9';
        w += (steadyDigits && digit) ? fd.digitAdvance : glyphFor(fd, c).advance;
    }
    return w;
}

void Framebuffer::devText(int x, int y, const std::string& str, uint16_t color, Font font) {
    const FontData& fd = fontData(scale_, font);
    int penX = x;
    for (char c : str) {
        const Glyph& g = glyphFor(fd, c);
        const uint8_t* src = fd.alpha + g.offset;
        for (int row = 0; row < g.h; row++) {
            for (int col = 0; col < g.w; col++) {
                int i = row * g.w + col;
                uint8_t a = (i & 1) ? (src[i >> 1] & 0x0F) : (src[i >> 1] >> 4);
                devBlend(penX + g.xoff + col, y + g.yoff + row, color, a);
            }
        }
        penX += g.advance;
    }
    if (audit_ && !str.empty()) {
        // Recorded in layout units, rounded inwards so touching boxes do not count as overlapping
        int left = unitsCeil(x), top = unitsCeil(y);
        int right = (penX * scale_.den) / scale_.num, bottom = ((y + fd.capHeight) * scale_.den) / scale_.num;
        textBoxes_.push_back({left, top, std::max(1, right - left), std::max(1, bottom - top), str});
    }
}

void Framebuffer::drawText(int x, int y, const std::string& str, uint16_t color, Font font) {
    devText(dev(x), dev(y), str, color, font);
}

void Framebuffer::drawTextRight(int xRight, int y, const std::string& str, uint16_t color, Font font) {
    devText(dev(xRight) - devTextWidth(str, font, false), dev(y), str, color, font);
}

void Framebuffer::drawTextCentered(int cx, int y, const std::string& str, uint16_t color, Font font) {
    devText(dev(cx) - devTextWidth(str, font, false) / 2, dev(y), str, color, font);
}

void Framebuffer::drawTextSteady(int xRight, int y, const std::string& str, uint16_t color, Font font) {
    devText(dev(xRight) - devTextWidth(str, font, true), dev(y), str, color, font);
}

int Framebuffer::textWidth(const std::string& str, Font font) const {
    return unitsCeil(devTextWidth(str, font, false));
}

int Framebuffer::maxNumericWidth(const std::string& str, Font font) const {
    return unitsCeil(devTextWidth(str, font, true));
}

int Framebuffer::textHeight(Font font) const {
    return unitsCeil(fontData(scale_, font).capHeight);
}

std::string Framebuffer::fitText(const std::string& str, int maxWidth, Font font) const {
    if (textWidth(str, font) <= maxWidth) return str;
    std::string s = str;
    while (!s.empty() && textWidth(s + "..", font) > maxWidth) {
        s.pop_back();
    }
    return s.empty() ? s : s + "..";
}
