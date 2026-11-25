#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "core/theme.hpp"

// Text styles. Screens pick a role; the font behind each role is defined in one place.
enum class Font {
    Small,   // labels and list text
    Title,   // page and section titles
    Large,   // headline values
};

// Ratio of device pixels to layout units. Pages are laid out once, in units of the
// 320x170 S1 panel; a larger display uses the same layout at a larger scale.
struct UiScale {
    int num = 1;
    int den = 1;
};

// A drawing surface. All coordinates passed in are layout units; the pixel buffer
// is in device pixels. At scale 1 the two are the same.
class Framebuffer {
public:
    // width and height in layout units
    Framebuffer(int width, int height, UiScale scale = UiScale());
    
    int width() const { return width_; }
    int height() const { return height_; }
    
    // The pixel buffer: panel byte order, row by row
    int deviceWidth() const { return devWidth_; }
    int deviceHeight() const { return devHeight_; }
    const uint16_t* pixels() const { return pixels_.data(); }
    
    void clear(uint16_t color = Theme::BLACK);
    
    // Filled shapes. Neighbouring rectangles meet without gaps at any scale.
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void fillRoundedRect(int x, int y, int w, int h, int radius, uint16_t color);
    void fillCircle(int cx, int cy, int r, uint16_t color);   // anti-aliased
    
    // Outlines and rules, one stroke wide
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
    void drawHLine(int x, int y, int w, uint16_t color);
    void drawVLine(int x, int y, int h, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    void drawRoundedRect(int x, int y, int w, int h, int radius, uint16_t color);
    
    // Segmented bar
    void drawSegmentedBar(int x, int y, int w, int h, float percent,
                          uint16_t fgColor, uint16_t bgColor, int segments = 10, int gap = 2);
    
    // Donut/ring gauge, filled clockwise from the top; anti-aliased
    void drawRingGauge(int cx, int cy, int outerR, int innerR, float percent,
                       uint16_t fgColor, uint16_t bgColor);
    
    // Stacked horizontal bar (multiple values)
    void drawStackedBar(int x, int y, int w, int h, 
                        const float* percents, const uint16_t* colors, int count,
                        uint16_t borderColor);
    
    // Sparkline (inline mini graph), auto-scaled to the value range
    void drawSparkline(int x, int y, int w, int h, const float* values, int count,
                       uint16_t lineColor, bool filled = false, uint16_t fillColor = 0);
    
    // Text. y is the top of the capital letters.
    void drawText(int x, int y, const std::string& str, uint16_t color, Font font = Font::Small);
    void drawTextRight(int xRight, int y, const std::string& str, uint16_t color,
                       Font font = Font::Small);
    void drawTextCentered(int cx, int y, const std::string& str, uint16_t color,
                          Font font = Font::Small);
    int textWidth(const std::string& str, Font font = Font::Small) const;
    // Widest str can get as its digits change (every digit counted at the widest digit)
    int maxNumericWidth(const std::string& str, Font font = Font::Small) const;
    // Right-aligned text whose left edge stays put while its digits change (clocks)
    void drawTextSteady(int xRight, int y, const std::string& str, uint16_t color,
                        Font font = Font::Small);
    int textHeight(Font font = Font::Small) const;
    // Shorten str with a trailing ".." until it fits maxWidth
    std::string fitText(const std::string& str, int maxWidth, Font font = Font::Small) const;
    
    // Layout audit: records the box of every drawText call so tools can detect
    // overlapping or out-of-bounds text.
    struct TextBox { int x, y, w, h; std::string text; };
    void setAudit(bool on) { audit_ = on; textBoxes_.clear(); }
    const std::vector<TextBox>& textBoxes() const { return textBoxes_; }

private:
    // Layout units -> device pixels, and back (rounding up)
    int dev(int v) const;
    float devLength(int v) const { return static_cast<float>(v) * scale_.num / scale_.den; }
    int unitsCeil(int devPixels) const { return (devPixels * scale_.den + scale_.num - 1) / scale_.num; }
    
    // Device-pixel primitives
    void devPixel(int x, int y, uint16_t color);
    void devBlend(int x, int y, uint16_t color, uint8_t alpha);   // alpha 0..15
    void devFill(int x0, int y0, int x1, int y1, uint16_t color); // x1, y1 exclusive
    int devTextWidth(const std::string& str, Font font, bool steadyDigits) const;
    void devText(int x, int y, const std::string& str, uint16_t color, Font font);
    
    int width_, height_;
    UiScale scale_;
    int devWidth_, devHeight_;
    int stroke_;                       // line thickness in device pixels
    std::vector<uint16_t> pixels_;
    bool audit_ = false;
    std::vector<TextBox> textBoxes_;
};
