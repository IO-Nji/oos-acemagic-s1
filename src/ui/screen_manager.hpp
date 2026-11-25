#pragma once
#include "core/framebuffer.hpp"
#include "core/layout.hpp"
#include "data/health.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace ui {
    inline uint16_t levelColor(Health::Level level) {
        switch (level) {
            case Health::Level::Critical: return Theme::CRIT;
            case Health::Level::Warning:  return Theme::WARN;
            default:                      return Theme::OK;
        }
    }
}

//=============================================================================
// ScreenBase - one dashboard page, plus the drawing helpers pages share
//=============================================================================
class ScreenBase {
public:
    explicit ScreenBase(const std::string& title) : title_(title) {}
    virtual ~ScreenBase() = default;
    
    // Draw the page into area (the region between header and footer)
    virtual void render(Framebuffer& fb, const Rect& area) = 0;
    
    // Called every frame, whether or not the page is showing; pages use it to
    // record history for their graphs
    virtual void update() {}
    
    const std::string& title() const { return title_; }
    
    // Space between cards, set by the device's layout
    void setCardGap(int gap) { cardGap_ = gap; }
    
protected:
    std::string title_;
    int cardGap_ = Layout::CARD_GAP;
    
    int cardGap() const { return cardGap_; }
    
    // Pages are designed for the S1's page area. On a taller one, a row designed
    // designH high grows in proportion...
    int rowHeight(int designH, const Rect& area) const {
        return designH * (area.h - cardGap_) / (Layout::MAIN_H - Layout::CARD_GAP);
    }
    
    // ...and a card's content, designed for a card designH high, stays centred in it.
    // Returns the y to lay the content out from.
    static int contentTop(const Rect& card, int designH) {
        return card.y + (card.h - designH) / 2;
    }
    
    // Shared metrics so every page uses the same rhythm
    static constexpr int CARD_PAD = 6;      // card edge to content, horizontal
    static constexpr int CARD_RADIUS = 3;
    static constexpr int DOT_RADIUS = 3;
    static constexpr int GAUGE_R = 18;
    
    // Tinted panel with a thin outline (oOS card)
    void drawCard(Framebuffer& fb, const Rect& r) {
        fb.fillRoundedRect(r.x, r.y, r.w, r.h, CARD_RADIUS, Theme::SURFACE);
        fb.drawRoundedRect(r.x, r.y, r.w, r.h, CARD_RADIUS, Theme::CARD_BORDER);
    }
    
    void drawSectionTitle(Framebuffer& fb, int x, int y, const std::string& text,
                          uint16_t color = Theme::TITLE) {
        fb.drawText(x, y, text, color, Font::Title);
    }
    
    // Section title with a rule underneath; returns the y where content starts
    int drawSectionHeader(Framebuffer& fb, int x, int y, int width, const std::string& text) {
        drawSectionTitle(fb, x, y, text);
        fb.drawHLine(x, y + 11, width, Theme::DIVIDER);
        return y + 15;
    }
    
    // Status dot whose centre lines up with text drawn at textY
    void drawStatusDot(Framebuffer& fb, int cx, int textY, uint16_t color) {
        fb.fillCircle(cx, textY + 4, DOT_RADIUS, color);
    }
    
    // Label on the left, value right-aligned
    void drawKeyValue(Framebuffer& fb, int x, int y, int width,
                      const std::string& key, const std::string& value,
                      uint16_t valueColor = Theme::VALUE) {
        fb.drawText(x, y, key, Theme::TEXT_LABEL);
        int room = width - fb.textWidth(key) - 6;
        fb.drawTextRight(x + width, y, fb.fitText(value, room), valueColor);
    }
    
    // Ring gauge with its reading in the middle. unit is drawn under the number;
    // three-digit readings drop the unit so they stay inside the ring.
    void drawGaugeRing(Framebuffer& fb, int cx, int cy, float percent,
                       const std::string& reading, const std::string& unit, uint16_t color) {
        static constexpr int THICKNESS = 4;
        fb.drawRingGauge(cx, cy, GAUGE_R, GAUGE_R - THICKNESS, percent, color, Theme::TRACK);
        if (reading.size() >= 3 || unit.empty()) {
            fb.drawTextCentered(cx, cy - 4, reading, color);
        } else {
            fb.drawTextCentered(cx, cy - 9, reading, color);
            fb.drawTextCentered(cx, cy + 2, unit, Theme::TEXT_LABEL);
        }
    }
    
    // Eight-segment bar across the bottom of a card
    void drawLevelBar(Framebuffer& fb, int x, int y, int width, float percent, uint16_t color) {
        const int segments = 8, gap = 2;
        fb.drawSegmentedBar(x, y, width + gap, 4, percent, color, Theme::TRACK, segments, gap);
    }
    
    static uint16_t percentToColor(float percent) {
        if (percent < 60.0f) return Theme::LEVEL_OK;
        if (percent < 85.0f) return Theme::LEVEL_WARN;
        return Theme::LEVEL_CRIT;
    }
    
    static uint16_t tempToColor(float temp) {
        if (temp < 60.0f) return Theme::LEVEL_OK;
        if (temp < health::TEMP_WARN) return Theme::LEVEL_WARN;
        return Theme::LEVEL_CRIT;
    }
    
    // Colour for a power reading relative to the CPU's rated power
    static uint16_t powerToColor(float watts, float ratedWatts) {
        if (watts > ratedWatts * 0.9f) return Theme::LEVEL_CRIT;
        if (watts > ratedWatts * 0.7f) return Theme::LEVEL_WARN;
        return Theme::LEVEL_OK;
    }
    
    static std::string formatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double val = static_cast<double>(bytes);
        while (val >= 1024 && unit < 4) {
            val /= 1024;
            unit++;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), val < 10 ? "%.1f%s" : "%.0f%s", val, units[unit]);
        return buf;
    }
    
    static std::string formatSpeed(uint64_t bytesPerSec) {
        return formatBytes(bytesPerSec) + "/s";
    }
};

//=============================================================================
// ScreenManager - rotates through the pages and draws the frame, header and
// footer around the one that is showing
//=============================================================================
class ScreenManager {
public:
    // health is shared with whoever evaluates it; the footer shows its summary
    ScreenManager(Framebuffer& fb, std::shared_ptr<const Health> health, const Chrome& chrome = Chrome())
        : fb_(fb), health_(std::move(health)), chrome_(chrome) {}
    
    void addPage(std::shared_ptr<ScreenBase> page) {
        page->setCardGap(chrome_.cardGap);
        pages_.push_back(std::move(page));
    }
    size_t pageCount() const { return pages_.size(); }
    
    // Time each page stays up; 0 disables rotation
    void setPageInterval(std::chrono::milliseconds interval) { interval_ = interval; }
    
    void setPage(size_t index) {
        if (index < pages_.size()) {
            current_ = index;
            lastSwitch_ = std::chrono::steady_clock::now();
        }
    }
    size_t currentPage() const { return current_; }
    
    // Manual navigation; the page that is chosen gets a full interval before rotating on
    void nextPage() { if (!pages_.empty()) setPage((current_ + 1) % pages_.size()); }
    void prevPage() { if (!pages_.empty()) setPage((current_ + pages_.size() - 1) % pages_.size()); }
    
    // While held, pages do not rotate on their own
    void setHold(bool hold) {
        hold_ = hold;
        lastSwitch_ = std::chrono::steady_clock::now();
    }
    bool hold() const { return hold_; }
    
    // Preview/test hook: fixed clock text and a steady activity dot
    void setTimeOverride(const std::string& t) { timeOverride_ = t; }
    
    void update() {
        for (auto& page : pages_) page->update();
    }
    
    void render() {
        fb_.clear(Theme::BG);
        if (chrome_.framed) {
            fb_.drawRoundedRect(Layout::OUTER_PADDING, Layout::OUTER_PADDING,
                                chrome_.width - 2 * Layout::OUTER_PADDING,
                                chrome_.height - 2 * Layout::OUTER_PADDING,
                                Layout::BORDER_RADIUS, Theme::BORDER);
        }
        if (pages_.empty()) return;
        
        auto now = std::chrono::steady_clock::now();
        if (!hold_ && interval_.count() > 0 && now - lastSwitch_ >= interval_) {
            current_ = (current_ + 1) % pages_.size();
            lastSwitch_ = now;
        }
        
        pages_[current_]->render(fb_, chrome_.page());
        drawHeader(pages_[current_]->title());
        drawFooter();
    }
    
private:
    Framebuffer& fb_;
    std::shared_ptr<const Health> health_;
    Chrome chrome_;
    std::vector<std::shared_ptr<ScreenBase>> pages_;
    size_t current_ = 0;
    std::chrono::milliseconds interval_{5000};
    bool hold_ = false;
    std::chrono::steady_clock::time_point lastSwitch_ = std::chrono::steady_clock::now();
    std::string timeOverride_;
    
    static constexpr int BAR_TEXT_Y = 3;   // text offset inside header and footer
    static constexpr int BAR_PAD = 5;
    static constexpr int PAGE_DOT_SPACING = 8;
    
    // Background of the header or footer: a flat strip inside the frame, or a card
    void drawBar(const Rect& r) {
        if (chrome_.framed) {
            fb_.fillRect(r.x, r.y, r.w, r.h, Theme::SURFACE);
        } else {
            fb_.fillRoundedRect(r.x, r.y, r.w, r.h, 3, Theme::SURFACE);
            fb_.drawRoundedRect(r.x, r.y, r.w, r.h, 3, Theme::CARD_BORDER);
        }
    }
    
    void drawHeader(const std::string& title) {
        const Rect r = chrome_.header();
        drawBar(r);
        fb_.drawText(r.x + BAR_PAD, r.y + BAR_TEXT_Y, title, Theme::TITLE, Font::Title);
        fb_.drawTextSteady(r.right() - BAR_PAD, r.y + BAR_TEXT_Y, clockText(), Theme::CLOCK);
        if (chrome_.framed) fb_.drawHLine(r.x, r.bottom(), r.w, Theme::DIVIDER);
    }
    
    void drawFooter() {
        const Rect r = chrome_.footer();
        const int midY = r.y + r.h / 2;
        if (chrome_.framed) fb_.drawHLine(r.x, r.y - 1, r.w, Theme::DIVIDER);
        drawBar(r);
        
        // Page indicator on the right: one dot per page, current page highlighted
        int dotsLeft = r.right() - BAR_PAD;
        if (pages_.size() > 1) {
            dotsLeft -= 2 + static_cast<int>(pages_.size() - 1) * PAGE_DOT_SPACING;
            for (size_t i = 0; i < pages_.size(); i++) {
                fb_.fillCircle(dotsLeft + static_cast<int>(i) * PAGE_DOT_SPACING, midY, 2,
                               i == current_ ? Theme::TITLE : Theme::TRACK);
            }
            dotsLeft -= 8;
        }
        
        // Health on the left: a dot that blinks once a second while the program is
        // alive, and the state in words
        bool ok = health_->level == Health::Level::Ok;
        uint16_t color = ui::levelColor(health_->level);
        bool lit = !timeOverride_.empty() || std::time(nullptr) % 2 == 0;
        fb_.fillCircle(r.x + BAR_PAD + 3, midY, 3, lit ? color : Theme::TRACK);
        
        int textX = r.x + BAR_PAD + 11;
        fb_.drawText(textX, r.y + BAR_TEXT_Y,
                     fb_.fitText(health_->summary(), dotsLeft - textX), ok ? Theme::TEXT_LABEL : color);
    }
    
    std::string clockText() const {
        if (!timeOverride_.empty()) return timeOverride_;
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        char buf[16];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
        return buf;
    }
};
