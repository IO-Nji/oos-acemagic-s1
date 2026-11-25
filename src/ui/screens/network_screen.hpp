#pragma once
#include "ui/screen_manager.hpp"
#include "data/providers/network_provider.hpp"
#include <algorithm>
#include <memory>
#include <vector>

//=============================================================================
// NetworkScreen - interfaces on the left, throughput graphs on the right
//=============================================================================
class NetworkScreen : public ScreenBase {
private:
    static constexpr int LEFT_PCT = 56;   // width share of the interface card

public:
    NetworkScreen(std::shared_ptr<NetworkDataProvider> provider)
        : ScreenBase("NETWORK"), provider_(provider) {
        rxHistory_.resize(MAX_HISTORY, 0);
        txHistory_.resize(MAX_HISTORY, 0);
    }
    
    // Add each newly collected sample to the history graphs
    void update() override {
        if (provider_->generation() == seenGeneration_) return;
        seenGeneration_ = provider_->generation();
        const auto data = provider_->data();
        
        rxHistory_.erase(rxHistory_.begin());
        txHistory_.erase(txHistory_.begin());
        rxHistory_.push_back(data.totalRxBps);
        txHistory_.push_back(data.totalTxBps);
        
        // Graph scale follows the largest value in view, with a 1 KB/s floor
        maxRx_ = std::max<uint64_t>(1024, *std::max_element(rxHistory_.begin(), rxHistory_.end()));
        maxTx_ = std::max<uint64_t>(1024, *std::max_element(txHistory_.begin(), txHistory_.end()));
    }
    
    void render(Framebuffer& fb, const Rect& area) override {
        const auto data = provider_->data();
        
        int leftW = (area.w - cardGap()) * LEFT_PCT / 100;
        Rect left(area.x, area.y, leftW, area.h);
        Rect right(area.x + leftW + cardGap(), area.y, area.w - leftW - cardGap(), area.h);
        
        renderInterfaceCard(fb, left, data);
        renderThroughputCard(fb, right, data);
    }
    
private:
    std::shared_ptr<NetworkDataProvider> provider_;
    
    static constexpr size_t MAX_HISTORY = 40;
    std::vector<uint64_t> rxHistory_;
    std::vector<uint64_t> txHistory_;
    uint64_t maxRx_ = 1024;
    uint64_t maxTx_ = 1024;
    uint64_t seenGeneration_ = 0;
    
    static constexpr int ITEM_H = 20;       // name line + speed line
    static constexpr int TOTALS_H = 22;     // RX and TX totals at the bottom
    
    void renderInterfaceCard(Framebuffer& fb, const Rect& r, const NetworkData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int halfX = x + innerW / 2;
        
        // Primary address
        drawSectionTitle(fb, x, r.y + 5, "IP");
        int ipX = x + fb.textWidth("IP", Font::Title) + 6;
        std::string ip = data.primaryIp.empty() ? "No Connection" : data.primaryIp;
        fb.drawText(ipX, r.y + 5, fb.fitText(ip, x + innerW - ipX), Theme::VALUE);
        
        fb.drawText(x, r.y + 18, "INTERFACES", Theme::TEXT_LABEL);
        fb.drawHLine(x, r.y + 29, innerW, Theme::DIVIDER);
        
        int listY = r.y + 33;
        int totalsY = r.bottom() - TOTALS_H - 1;
        int maxItems = (totalsY - listY) / ITEM_H;
        int shown = 0;
        char buf[32];
        
        for (const auto& iface : data.interfaces) {
            if (shown >= maxItems) break;
            
            int itemY = listY + shown * ITEM_H;
            
            drawStatusDot(fb, x + DOT_RADIUS, itemY, iface.isUp ? Theme::OK : Theme::CRIT);
            // Wi-Fi interfaces show their signal level beside the name
            int nameW = innerW - 11;
            if (iface.signalDbm < 0) {
                std::string signal = std::to_string(iface.signalDbm) + "dBm";
                uint16_t color = iface.signalDbm >= -67 ? Theme::OK
                               : iface.signalDbm >= -80 ? Theme::WARN : Theme::CRIT;
                fb.drawTextRight(x + innerW, itemY, signal, color);
                nameW -= fb.textWidth(signal) + 4;
            }
            fb.drawText(x + 11, itemY, fb.fitText(iface.name, nameW), Theme::TEXT);
            
            snprintf(buf, sizeof(buf), "D:%s", formatSpeed(iface.rxBps).c_str());
            fb.drawText(x, itemY + 10, buf, Theme::DOWN);
            
            snprintf(buf, sizeof(buf), "U:%s", formatSpeed(iface.txBps).c_str());
            fb.drawText(halfX, itemY + 10, buf, Theme::UP);
            
            shown++;
        }
        
        if (shown == 0) {
            fb.drawText(x, listY, "No interfaces", Theme::TEXT_LABEL);
        }
        
        // Totals since boot
        snprintf(buf, sizeof(buf), "RX:%s", formatBytes(data.totalRxBytes).c_str());
        fb.drawText(x, totalsY, buf, Theme::DOWN);
        snprintf(buf, sizeof(buf), "TX:%s", formatBytes(data.totalTxBytes).c_str());
        fb.drawText(x, totalsY + 11, buf, Theme::UP);
    }
    
    void renderThroughputCard(Framebuffer& fb, const Rect& r, const NetworkData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        
        // Two stacked blocks: label line, then graph
        const int labelH = 12, blockGap = 6, topPad = 5, bottomPad = 5;
        int graphH = (r.h - topPad - bottomPad - blockGap - 2 * labelH) / 2;
        
        int y = r.y + topPad;
        drawSectionTitle(fb, x, y, "DOWN", Theme::DOWN);
        fb.drawTextRight(x + innerW, y, formatSpeed(data.totalRxBps), Theme::VALUE);
        drawHistoryGraph(fb, Rect(x, y + labelH, innerW, graphH),
                         rxHistory_, maxRx_, Theme::DOWN, Theme::DOWN_FILL);
        
        y += labelH + graphH + blockGap;
        drawSectionTitle(fb, x, y, "UP", Theme::UP);
        fb.drawTextRight(x + innerW, y, formatSpeed(data.totalTxBps), Theme::VALUE);
        drawHistoryGraph(fb, Rect(x, y + labelH, innerW, graphH),
                         txHistory_, maxTx_, Theme::UP, Theme::UP_FILL);
    }
    
    void drawHistoryGraph(Framebuffer& fb, const Rect& g,
                          const std::vector<uint64_t>& history, uint64_t maxVal,
                          uint16_t lineColor, uint16_t fillColor) {
        fb.fillRect(g.x, g.y, g.w, g.h, Theme::GRAPH_BG);
        fb.drawRect(g.x, g.y, g.w, g.h, Theme::CARD_BORDER);
        
        int count = static_cast<int>(history.size());
        if (count < 2 || maxVal == 0) return;
        
        // Plot inside the outline
        int left = g.x + 1, bottom = g.bottom() - 2;
        int plotW = g.w - 3, plotH = g.h - 3;
        float xStep = static_cast<float>(plotW) / (count - 1);
        
        for (int i = 0; i < count - 1; i++) {
            float ratio1 = std::min(1.0f, static_cast<float>(history[i]) / maxVal);
            float ratio2 = std::min(1.0f, static_cast<float>(history[i + 1]) / maxVal);
            
            int x1 = left + static_cast<int>(i * xStep);
            int x2 = left + static_cast<int>((i + 1) * xStep);
            int y1 = bottom - static_cast<int>(ratio1 * plotH);
            int y2 = bottom - static_cast<int>(ratio2 * plotH);
            
            for (int px = x1; px <= x2; px++) {
                float t = (x2 == x1) ? 0 : static_cast<float>(px - x1) / (x2 - x1);
                int py = y1 + static_cast<int>(t * (y2 - y1));
                fb.fillRect(px, py, 1, bottom - py + 1, fillColor);
            }
            
            fb.drawLine(x1, y1, x2, y2, lineColor);
        }
    }
};
