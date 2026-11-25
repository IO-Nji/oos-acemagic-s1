#pragma once
#include "ui/screen_manager.hpp"
#include "data/providers/identity_provider.hpp"
#include <ctime>
#include <memory>

//=============================================================================
// IdentityScreen - hostname and clock on top; system details, sessions and
// machine health below
//=============================================================================
class IdentityScreen : public ScreenBase {
private:
    static constexpr int BANNER_H = 34;
    static constexpr int LEFT_PCT = 59;     // width share of the system card
    static constexpr int LINE = 11;         // label to value
    static constexpr int GROUP = 13;        // value to next label

public:
    IdentityScreen(std::shared_ptr<IdentityDataProvider> provider, std::shared_ptr<const Health> health)
        : ScreenBase("IDENTITY"), provider_(std::move(provider)), health_(std::move(health)) {}
    
    void render(Framebuffer& fb, const Rect& area) override {
        auto data = provider_->data();
        
        // The clock is read at draw time so it ticks with the header, not with the
        // collection interval
        if (!provider_->frozen()) {
            std::time_t now = std::time(nullptr);
            std::tm tm{};
            localtime_r(&now, &tm);
            char buf[16];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
            data.currentTime = buf;
            std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
            data.currentDate = buf;
        }
        
        const int bannerH = rowHeight(BANNER_H, area);
        Rect banner(area.x, area.y, area.w, bannerH);
        int lowerY = area.y + bannerH + cardGap();
        int lowerH = area.h - bannerH - cardGap();
        int leftW = (area.w - cardGap()) * LEFT_PCT / 100;
        
        renderBanner(fb, banner, data);
        renderSystemInfo(fb, Rect(area.x, lowerY, leftW, lowerH), data);
        renderSessionInfo(fb, Rect(area.x + leftW + cardGap(), lowerY,
                                   area.w - leftW - cardGap(), lowerH), data);
    }
    
private:
    std::shared_ptr<IdentityDataProvider> provider_;
    std::shared_ptr<const Health> health_;
    
    void renderBanner(Framebuffer& fb, const Rect& r, const IdentityData& data) {
        drawCard(fb, r);
        const int top = contentTop(r, BANNER_H);
        int x = r.x + CARD_PAD;
        int right = r.right() - CARD_PAD;
        
        // Time and date on the right
        int timeW = fb.maxNumericWidth(data.currentTime, Font::Large);
        fb.drawTextSteady(right, top + 5, data.currentTime, Theme::VALUE, Font::Large);
        fb.drawTextRight(right, top + 22, data.currentDate, Theme::TEXT_LABEL);
        
        // Hostname takes the remaining width, centred on the card height
        int hostW = right - timeW - 10 - x;
        fb.drawText(x, r.y + (r.h - fb.textHeight(Font::Large)) / 2,
                    fb.fitText(data.hostname, hostW, Font::Large), Theme::TITLE, Font::Large);
    }
    
    void renderSystemInfo(Framebuffer& fb, const Rect& r, const IdentityData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int y = r.y + 6;
        
        fb.drawText(x, y, "OS", Theme::TEXT_LABEL);
        fb.drawText(x, y + LINE, fb.fitText(data.osName, innerW), Theme::VALUE);
        y += LINE + GROUP;
        
        fb.drawText(x, y, "KERNEL", Theme::TEXT_LABEL);
        fb.drawText(x, y + LINE, fb.fitText(data.kernelVersion, innerW), Theme::VALUE_ALT);
        y += LINE + GROUP;
        
        drawKeyValue(fb, x, y, innerW, "ARCH", data.architecture, Theme::VALUE_DIM);
        y += LINE + 1;
        
        drawKeyValue(fb, x, y, innerW, "UPTIME", data.uptimeStr);
    }
    
    void renderSessionInfo(Framebuffer& fb, const Rect& r, const IdentityData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int y = drawSectionHeader(fb, x, r.y + 6, innerW, "SESSIONS");
        
        drawKeyValue(fb, x, y, innerW, "USERS", std::to_string(data.loggedInUsers));
        y += LINE + 1;
        
        drawKeyValue(fb, x, y, innerW, "SSH", std::to_string(data.remoteSessions),
                     data.remoteSessions > 0 ? Theme::WARN : Theme::VALUE_DIM);
        y += LINE + 1;
        
        // Machine health: state on one line, then the first alerts
        const char* word = health_->level == Health::Level::Ok ? "OK"
                         : health_->level == Health::Level::Warning ? "WARN" : "CRIT";
        uint16_t color = ui::levelColor(health_->level);
        fb.drawText(x, y, "STATUS", Theme::TEXT_LABEL);
        fb.drawTextRight(x + innerW, y, word, color);
        drawStatusDot(fb, x + innerW - fb.textWidth(word) - DOT_RADIUS - 4, y, color);
        y += LINE + 3;
        
        if (health_->alerts.empty()) {
            fb.drawText(x, y, "No alerts", Theme::TEXT_FAINT);
        }
        for (size_t i = 0; i < health_->alerts.size() && i < 2; i++) {
            fb.drawText(x, y + static_cast<int>(i) * LINE, fb.fitText(health_->alerts[i], innerW), color);
        }
    }
};
