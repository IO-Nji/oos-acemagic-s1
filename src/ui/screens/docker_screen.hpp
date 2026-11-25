#pragma once
#include "ui/screen_manager.hpp"
#include "data/providers/docker_provider.hpp"
#include <memory>
#include <utility>

//=============================================================================
// DockerScreen - container counts on top, container list below
//=============================================================================
class DockerScreen : public ScreenBase {
private:
    static constexpr int SUMMARY_H = 26;
    static constexpr int ITEM_H = 23;       // name line + resource line
    static constexpr int MEM_COL = 78;      // x offset of the memory column in a row

public:
    explicit DockerScreen(std::shared_ptr<DockerDataProvider> provider)
        : ScreenBase("DOCKER"), provider_(std::move(provider)) {}
    
    void render(Framebuffer& fb, const Rect& area) override {
        const auto data = provider_->data();
        
        if (!data.available) {
            drawCard(fb, area);
            fb.drawText(area.x + CARD_PAD, area.y + 7, "Docker not available", Theme::TEXT_LABEL);
            return;
        }
        
        const int summaryH = rowHeight(SUMMARY_H, area);
        Rect summary(area.x, area.y, area.w, summaryH);
        Rect list(area.x, area.y + summaryH + cardGap(), area.w, area.h - summaryH - cardGap());
        
        renderSummary(fb, summary, data);
        renderContainers(fb, list, data);
    }
    
private:
    std::shared_ptr<DockerDataProvider> provider_;
    
    void renderSummary(Framebuffer& fb, const Rect& r, const DockerData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int right = r.right() - CARD_PAD;
        int bigY = r.y + (r.h - fb.textHeight(Font::Large)) / 2;
        int smallY = r.y + (r.h - fb.textHeight()) / 2;
        
        // Running count, label, total
        std::string running = std::to_string(data.running);
        fb.drawText(x, bigY, running, Theme::OK, Font::Large);
        x += fb.textWidth(running, Font::Large) + 6;
        
        fb.drawText(x, smallY, "RUNNING", Theme::TEXT_LABEL);
        x += fb.textWidth("RUNNING") + 4;
        
        fb.drawText(x, smallY, "/" + std::to_string(data.total), Theme::VALUE_DIM);
        
        // Exceptions on the right, most serious first; one line is centred, two are stacked
        std::vector<std::pair<std::string, uint16_t>> notes;
        if (data.unhealthy > 0) notes.push_back({"UNHEALTHY:" + std::to_string(data.unhealthy), Theme::CRIT});
        if (data.stopped > 0) notes.push_back({"STOP:" + std::to_string(data.stopped), Theme::WARN});
        if (data.paused > 0) notes.push_back({"PAUSE:" + std::to_string(data.paused), Theme::WARN});
        
        if (notes.size() >= 2) {
            fb.drawTextRight(right, smallY - 5, notes[0].first, notes[0].second);
            fb.drawTextRight(right, smallY + 5, notes[1].first, notes[1].second);
        } else if (notes.size() == 1) {
            fb.drawTextRight(right, smallY, notes[0].first, notes[0].second);
        }
    }
    
    static uint16_t statusColor(const DockerContainerData& c) {
        if (DockerDataProvider::needsAttention(c) || c.status == "exited") return Theme::CRIT;
        if (c.status == "running") return Theme::OK;
        if (c.status == "paused") return Theme::WARN;
        if (c.status == "created") return Theme::INFO;
        return Theme::TEXT_LABEL;
    }
    
    // "registry/path/name:tag" -> "name:tag"
    static std::string shortImage(const std::string& image) {
        size_t lastSlash = image.rfind('/');
        return lastSlash == std::string::npos ? image : image.substr(lastSlash + 1);
    }
    
    static std::string shortDuration(int secs) {
        if (secs >= 86400) return std::to_string(secs / 86400) + "d";
        if (secs >= 3600) return std::to_string(secs / 3600) + "h";
        return std::to_string(secs / 60) + "m";
    }
    
    void renderContainers(Framebuffer& fb, const Rect& r, const DockerData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int right = x + innerW;
        int textX = x + 11;
        int listY = r.y + 6;
        
        if (data.containers.empty()) {
            fb.drawText(x, listY, "No containers", Theme::TEXT_LABEL);
            return;
        }
        
        // Keep the last line for "+N more" when the list does not fit
        int total = static_cast<int>(data.containers.size());
        int maxItems = (r.bottom() - 2 - listY) / ITEM_H;
        if (total > maxItems) {
            maxItems = (r.bottom() - 4 - fb.textHeight() - 2 - listY) / ITEM_H;
        }
        
        char buf[64];
        int shown = 0;
        for (const auto& container : data.containers) {
            if (shown >= maxItems) break;
            
            int itemY = listY + shown * ITEM_H;
            
            drawStatusDot(fb, x + DOT_RADIUS, itemY, statusColor(container));
            
            // Image on the right takes at most 40 % of the row; the name gets the rest
            std::string image = fb.fitText(shortImage(container.image), innerW * 40 / 100);
            int imageW = fb.textWidth(image);
            fb.drawTextRight(right, itemY, image, Theme::TEXT_LABEL);
            fb.drawText(textX, itemY, fb.fitText(container.name, right - imageW - 8 - textX),
                        Theme::TEXT);
            
            int row2Y = itemY + 11;
            if (container.status == "running") {
                snprintf(buf, sizeof(buf), "CPU:%.1f%%", container.cpuPercent);
                fb.drawText(textX, row2Y, buf, percentToColor(container.cpuPercent));
                
                snprintf(buf, sizeof(buf), "MEM:%dM", container.memUsageMB);
                fb.drawText(textX + MEM_COL, row2Y, buf, Theme::VALUE_ALT);
                
                if (container.unhealthy) {
                    fb.drawTextRight(right, row2Y, "unhealthy", Theme::CRIT);
                } else {
                    fb.drawTextRight(right, row2Y, shortDuration(container.uptimeSeconds), Theme::VALUE_DIM);
                }
            } else {
                fb.drawText(textX, row2Y, container.status, Theme::TEXT_LABEL);
            }
            
            shown++;
        }
        
        if (total > shown) {
            snprintf(buf, sizeof(buf), "+%d more", total - shown);
            fb.drawText(x, r.bottom() - 4 - fb.textHeight(), buf, Theme::TEXT_LABEL);
        }
    }
};
