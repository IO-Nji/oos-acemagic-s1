#pragma once
#include "ui/screen_manager.hpp"
#include "data/providers/services_provider.hpp"
#include <memory>

//=============================================================================
// ServicesScreen - systemd service counts on top, watched services below
//=============================================================================
class ServicesScreen : public ScreenBase {
private:
    static constexpr int SUMMARY_H = 26;
    static constexpr int ITEM_H = 14;
    static constexpr int COL_GAP = 8;

public:
    ServicesScreen(std::shared_ptr<ServicesDataProvider> provider)
        : ScreenBase("SERVICES"), provider_(provider) {}
    
    void render(Framebuffer& fb, const Rect& area) override {
        const auto data = provider_->data();
        
        Rect summary(area.x, area.y, area.w, SUMMARY_H);
        Rect list(area.x, area.y + SUMMARY_H + CARD_GAP, area.w, area.h - SUMMARY_H - CARD_GAP);
        
        renderSummary(fb, summary, data);
        renderServices(fb, list, data);
    }
    
private:
    std::shared_ptr<ServicesDataProvider> provider_;
    
    void renderSummary(Framebuffer& fb, const Rect& r, const ServicesData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int right = r.right() - CARD_PAD;
        int bigY = r.y + (r.h - fb.textHeight(Font::Large)) / 2;
        int smallY = r.y + (r.h - fb.textHeight()) / 2;
        int total = static_cast<int>(data.services.size());
        char buf[64];
        
        // Active count, label, total
        snprintf(buf, sizeof(buf), "%d", data.activeCount);
        fb.drawText(x, bigY, buf, Theme::OK, Font::Large);
        x += fb.textWidth(buf, Font::Large) + 6;
        
        fb.drawText(x, smallY, "ACTIVE", Theme::TEXT_LABEL);
        x += fb.textWidth("ACTIVE") + 4;
        
        snprintf(buf, sizeof(buf), "/%d", total);
        fb.drawText(x, smallY, buf, Theme::VALUE_DIM);
        
        // Source tag at the far right
        fb.drawTextRight(right, smallY, "systemd", Theme::TEXT_FAINT);
        right -= fb.textWidth("systemd") + 10;
        
        // Failed and inactive counts; one line is centred, two are stacked
        std::string failed, inactive;
        if (data.failedCount > 0) failed = "FAILED:" + std::to_string(data.failedCount);
        int inactiveCount = total - data.activeCount - data.failedCount;
        if (inactiveCount > 0) inactive = "INACTIVE:" + std::to_string(inactiveCount);
        
        if (!failed.empty() && !inactive.empty()) {
            fb.drawTextRight(right, r.y + 4, failed, Theme::CRIT);
            fb.drawTextRight(right, r.y + 14, inactive, Theme::WARN);
        } else if (!failed.empty()) {
            fb.drawTextRight(right, smallY, failed, Theme::CRIT);
        } else if (!inactive.empty()) {
            fb.drawTextRight(right, smallY, inactive, Theme::WARN);
        }
    }
    
    void renderServices(Framebuffer& fb, const Rect& r, const ServicesData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int listY = r.y + 7;
        
        if (data.services.empty()) {
            fb.drawText(x, listY, "No services monitored", Theme::TEXT_LABEL);
            fb.drawText(x, listY + 12, "Add services to watch list", Theme::TEXT_LABEL);
            return;
        }
        
        // Two columns; keep the last line for "+N more" when the list does not fit
        int total = static_cast<int>(data.services.size());
        int rows = (r.bottom() - 3 - listY) / ITEM_H;
        if (total > rows * 2) {
            rows = (r.bottom() - 4 - fb.textHeight() - 2 - listY) / ITEM_H;
        }
        int colW = (innerW - COL_GAP) / 2;
        int maxItems = rows * 2;
        
        int shown = 0;
        for (const auto& service : data.services) {
            if (shown >= maxItems) break;
            
            int itemX = x + (shown / rows) * (colW + COL_GAP);
            int itemY = listY + (shown % rows) * ITEM_H;
            
            // Active services get a dot; other states get a symbol and their own colour
            uint16_t color = Theme::TEXT_LABEL;
            const char* symbol = "?";
            bool active = false;
            
            if (service.status == "active") {
                active = true;
            } else if (service.status == "inactive") {
                color = Theme::WARN;
                symbol = "-";
            } else if (service.status == "failed") {
                color = Theme::CRIT;
                symbol = "X";
            } else if (service.status == "activating" || service.status == "reloading") {
                color = Theme::INFO;
                symbol = ">";
            } else if (service.status == "deactivating") {
                color = Theme::WARN;
                symbol = "<";
            }
            
            if (active) {
                drawStatusDot(fb, itemX + DOT_RADIUS, itemY, Theme::OK);
            } else {
                fb.drawTextCentered(itemX + DOT_RADIUS, itemY, symbol, color);
            }
            
            std::string name = service.name;
            const std::string suffix = ".service";
            if (name.length() > suffix.length() &&
                name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0) {
                name.erase(name.length() - suffix.length());
            }
            
            fb.drawText(itemX + 11, itemY, fb.fitText(name, colW - 11),
                        active ? Theme::TEXT : color);
            
            shown++;
        }
        
        if (total > shown) {
            char buf[32];
            snprintf(buf, sizeof(buf), "+%d more", total - shown);
            fb.drawText(x, r.bottom() - 4 - fb.textHeight(), buf, Theme::TEXT_LABEL);
        }
    }
};
