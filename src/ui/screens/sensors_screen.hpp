#pragma once
#include "ui/screen_manager.hpp"
#include "data/providers/sensors_provider.hpp"
#include <memory>
#include <vector>

//=============================================================================
// SensorsScreen - temperature gauges on top; fans, power and temperature list below
//=============================================================================
class SensorsScreen : public ScreenBase {
private:
    static constexpr int ROW1_H = 56;
    static constexpr int LIST_PITCH = 11;
    static constexpr float FALLBACK_RATED_W = 15.0f;   // when the CPU does not report one
    
public:
    explicit SensorsScreen(std::shared_ptr<SensorsDataProvider> provider)
        : ScreenBase("SENSORS"), provider_(std::move(provider)) {
        cpuHistory_.resize(HISTORY_SIZE, 0);
        secondHistory_.resize(HISTORY_SIZE, 0);
    }
    
    // Add each newly collected sample to the history graphs
    void update() override {
        if (provider_->generation() == seenGeneration_) return;
        seenGeneration_ = provider_->generation();
        const auto data = provider_->data();
        
        cpuHistory_.erase(cpuHistory_.begin());
        cpuHistory_.push_back(data.cpuTempC);
        secondHistory_.erase(secondHistory_.begin());
        secondHistory_.push_back(secondGauge(data).temp);
    }
    
    void render(Framebuffer& fb, const Rect& area) override {
        const auto data = provider_->data();
        const SecondGauge second = secondGauge(data);
        
        Rect row1(area.x, area.y, area.w, ROW1_H);
        Rect row2(area.x, area.y + ROW1_H + CARD_GAP, area.w, area.h - ROW1_H - CARD_GAP);
        
        renderTempCard(fb, row1.column(0, 2, CARD_GAP), "CPU", data.cpuTempC, cpuHistory_);
        renderTempCard(fb, row1.column(1, 2, CARD_GAP), second.label, second.temp, secondHistory_);
        
        renderFansCard(fb, row2.column(0, 3, CARD_GAP), data);
        renderPowerCard(fb, row2.column(1, 3, CARD_GAP), data);
        renderTempListCard(fb, row2.column(2, 3, CARD_GAP), data, second.isSsd);
    }
    
private:
    std::shared_ptr<SensorsDataProvider> provider_;
    
    static constexpr int HISTORY_SIZE = 20;
    std::vector<float> cpuHistory_;
    std::vector<float> secondHistory_;
    uint64_t seenGeneration_ = 0;
    
    // The second gauge shows the GPU when it reports a temperature, otherwise the SSD
    struct SecondGauge {
        const char* label;
        float temp;
        bool isSsd;
    };
    static SecondGauge secondGauge(const SensorsData& data) {
        if (data.gpuTempC <= 0 && data.ssdTempC > 0) return {"SSD", data.ssdTempC, true};
        return {"GPU", data.gpuTempC, false};
    }
    
    void renderTempCard(Framebuffer& fb, const Rect& r, const std::string& label,
                        float temp, const std::vector<float>& history) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        drawSectionTitle(fb, x, r.y + 5, label);
        
        int cx = x + GAUGE_R;
        int cy = r.y + 34;
        
        if (temp <= 0) {
            fb.drawRingGauge(cx, cy, GAUGE_R, GAUGE_R - 4, 0, Theme::TRACK, Theme::TRACK);
            fb.drawTextCentered(cx, cy - 4, "N/A", Theme::TEXT_LABEL);
            return;
        }
        
        // 20-100 C mapped onto the ring
        float percent = std::max(0.0f, std::min(100.0f, (temp - 20.0f) / 80.0f * 100.0f));
        uint16_t tempColor = tempToColor(temp);
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", temp);
        drawGaugeRing(fb, cx, cy, percent, buf, "C", tempColor);
        
        // History to the right of the ring
        int sparkX = x + GAUGE_R * 2 + 8;
        int sparkW = r.right() - CARD_PAD - sparkX;
        fb.drawSparkline(sparkX, cy - GAUGE_R + 2, sparkW - 1, GAUGE_R * 2 - 4,
                         history.data(), HISTORY_SIZE, tempColor, true, Theme::GRAPH_FILL);
    }
    
    void renderFansCard(Framebuffer& fb, const Rect& r, const SensorsData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int listY = drawSectionHeader(fb, x, r.y + 5, innerW, "FANS");
        
        if (data.fans.empty()) {
            // The S1 does not expose its fan to the operating system
            fb.drawText(x, listY, "N/A", Theme::TEXT_LABEL);
            fb.drawText(x, listY + LIST_PITCH, "No sensor", Theme::TEXT_FAINT);
            return;
        }
        
        int maxItems = (r.bottom() - 3 - listY) / LIST_PITCH;
        int dotX = x + innerW - DOT_RADIUS - 1;
        int valueRight = dotX - DOT_RADIUS - 4;
        
        for (int i = 0; i < maxItems && i < static_cast<int>(data.fans.size()); i++) {
            const auto& fan = data.fans[i];
            int itemY = listY + i * LIST_PITCH;
            
            int rpm = static_cast<int>(fan.value);
            std::string value = rpm > 0 ? std::to_string(rpm) : "OFF";
            int nameW = valueRight - fb.textWidth(value) - 4 - x;
            
            fb.drawText(x, itemY, fb.fitText(fan.name, nameW), Theme::TEXT_LABEL);
            fb.drawTextRight(valueRight, itemY, value, rpm > 0 ? Theme::VALUE : Theme::OFF);
            drawStatusDot(fb, dotX, itemY, rpm > 0 ? Theme::OK : Theme::TRACK);
        }
    }
    
    void renderPowerCard(Framebuffer& fb, const Rect& r, const SensorsData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int top = drawSectionHeader(fb, x, r.y + 5, innerW, "POWER");
        
        if (data.totalPowerW <= 0) {
            fb.drawText(x, top, "N/A", Theme::TEXT_LABEL);
            fb.drawText(x, top + LIST_PITCH, "No sensor", Theme::TEXT_FAINT);
            return;
        }
        
        char buf[32];
        float rated = data.tdpW > 0 ? data.tdpW : FALLBACK_RATED_W;
        uint16_t powerColor = powerToColor(data.totalPowerW, rated);
        
        // Total, with the unit sitting on the same baseline
        int unitW = fb.textWidth("W") + 3;
        snprintf(buf, sizeof(buf), "%.1f", data.totalPowerW);
        std::string total = fb.fitText(buf, innerW - unitW, Font::Large);
        fb.drawText(x, top, total, powerColor, Font::Large);
        int bigH = fb.textHeight(Font::Large);
        fb.drawText(x + fb.textWidth(total, Font::Large) + 3, top + bigH - fb.textHeight(),
                    "W", Theme::TEXT_LABEL);
        
        // Share of the CPU's rated power
        int barY = top + bigH + 4;
        drawLevelBar(fb, x, barY, innerW, std::min(100.0f, data.totalPowerW / rated * 100.0f), powerColor);
        
        // Per-domain readings, as many as fit
        int listY = barY + 8;
        int maxItems = (r.bottom() - 3 - listY + (LIST_PITCH - fb.textHeight())) / LIST_PITCH;
        for (int i = 0; i < maxItems && i < static_cast<int>(data.power.size()); i++) {
            const auto& domain = data.power[i];
            int itemY = listY + i * LIST_PITCH;
            
            snprintf(buf, sizeof(buf), "%.1fW", domain.value);
            int nameW = innerW - fb.textWidth(buf) - 4;
            fb.drawText(x, itemY, fb.fitText(domain.name, nameW), Theme::TEXT_LABEL);
            fb.drawTextRight(x + innerW, itemY, buf, Theme::VALUE_ALT);
        }
    }
    
    void renderTempListCard(Framebuffer& fb, const Rect& r, const SensorsData& data, bool ssdOnGauge) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int listY = drawSectionHeader(fb, x, r.y + 5, innerW, "TEMPS");
        
        int maxItems = (r.bottom() - 3 - listY) / LIST_PITCH;
        int shown = 0;
        
        for (const auto& temp : data.temperatures) {
            if (shown >= maxItems) break;
            
            // Leave out what the gauges above already show
            if (temp.name.find("Package") != std::string::npos ||
                temp.name.find("Tctl") != std::string::npos ||
                (ssdOnGauge && temp.name == "Composite")) {
                continue;
            }
            
            int itemY = listY + shown * LIST_PITCH;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0fC", temp.value);
            int nameW = innerW - fb.textWidth(buf) - 4;
            
            fb.drawText(x, itemY, fb.fitText(temp.name, nameW), Theme::TEXT_LABEL);
            fb.drawTextRight(x + innerW, itemY, buf, tempToColor(temp.value));
            
            shown++;
        }
        
        if (shown == 0) {
            fb.drawText(x, listY, "None", Theme::TEXT_LABEL);
        }
    }
};
