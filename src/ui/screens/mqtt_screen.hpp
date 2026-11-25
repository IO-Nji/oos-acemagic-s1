#pragma once
#include "ui/screen_manager.hpp"
#include "data/providers/mqtt_monitor.hpp"
#include <memory>
#include <utility>

//=============================================================================
// MqttScreen - broker status on top, a running log of messages below
//=============================================================================
class MqttScreen : public ScreenBase {
private:
    static constexpr int SUMMARY_H = 26;
    static constexpr int LINE_PITCH = 11;
    static constexpr int TOPIC_PCT = 45;     // most of a line a topic may take

public:
    explicit MqttScreen(std::shared_ptr<MqttMonitor> monitor)
        : ScreenBase("MQTT"), monitor_(std::move(monitor)) {}
    
    void render(Framebuffer& fb, const Rect& area) override {
        const auto data = monitor_->data();
        
        const int summaryH = rowHeight(SUMMARY_H, area);
        Rect summary(area.x, area.y, area.w, summaryH);
        Rect log(area.x, area.y + summaryH + cardGap(), area.w, area.h - summaryH - cardGap());
        
        renderSummary(fb, summary, data);
        renderLog(fb, log, data);
    }
    
private:
    std::shared_ptr<MqttMonitor> monitor_;
    
    static std::string perMinute(float rate) {
        char buf[24];
        snprintf(buf, sizeof(buf), rate < 10 ? "%.1f/m" : "%.0f/m", rate);
        return buf;
    }
    
    static std::string duration(long secs) {
        if (secs >= 86400) return std::to_string(secs / 86400) + "d " + std::to_string(secs % 86400 / 3600) + "h";
        if (secs >= 3600) return std::to_string(secs / 3600) + "h " + std::to_string(secs % 3600 / 60) + "m";
        return std::to_string(secs / 60) + "m";
    }
    
    void renderSummary(Framebuffer& fb, const Rect& r, const MqttData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int right = r.right() - CARD_PAD;
        int bigY = r.y + (r.h - fb.textHeight(Font::Large)) / 2;
        int smallY = r.y + (r.h - fb.textHeight()) / 2;
        
        if (data.state != MqttData::State::Online) {
            bool offline = data.state == MqttData::State::Offline;
            drawStatusDot(fb, x + DOT_RADIUS, smallY, offline ? Theme::CRIT : Theme::WARN);
            fb.drawText(x + 11, smallY, offline ? "BROKER OFFLINE" : "CONNECTING", offline ? Theme::CRIT : Theme::WARN);
            fb.drawTextRight(right, smallY, fb.fitText(data.broker, r.w / 2), Theme::TEXT_LABEL);
            return;
        }
        
        // Connected clients, label, and how many the broker knows in total
        std::string clients = data.clientsConnected >= 0 ? std::to_string(data.clientsConnected) : "-";
        fb.drawText(x, bigY, clients, Theme::OK, Font::Large);
        x += fb.textWidth(clients, Font::Large) + 6;
        
        fb.drawText(x, smallY, "CLIENTS", Theme::TEXT_LABEL);
        x += fb.textWidth("CLIENTS") + 4;
        if (data.clientsTotal >= 0) {
            fb.drawText(x, smallY, "/" + std::to_string(data.clientsTotal), Theme::VALUE_DIM);
        }
        
        // Message rates over the last minute, stacked on the right
        if (data.receivedPerMin >= 0 && data.sentPerMin >= 0) {
            std::string in = perMinute(data.receivedPerMin), out = perMinute(data.sentPerMin);
            int valueW = std::max(fb.textWidth(in), fb.textWidth(out));
            int labelRight = right - valueW - 4;
            fb.drawTextRight(right, smallY - 5, in, Theme::DOWN);
            fb.drawTextRight(right, smallY + 5, out, Theme::UP);
            fb.drawTextRight(labelRight, smallY - 5, "IN", Theme::TEXT_LABEL);
            fb.drawTextRight(labelRight, smallY + 5, "OUT", Theme::TEXT_LABEL);
        }
    }
    
    void renderLog(Framebuffer& fb, const Rect& r, const MqttData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        int listY = drawSectionHeader(fb, x, r.y + 5, innerW, "MESSAGES");
        
        // Beside the title: topics seen, and how long the broker has been up
        std::string info = std::to_string(data.topicCount) + (data.topicCount == 1 ? " topic" : " topics");
        if (data.uptimeSeconds >= 0) info += "  up " + duration(data.uptimeSeconds);
        fb.drawTextRight(x + innerW, r.y + 5,
                         fb.fitText(info, innerW - fb.textWidth("MESSAGES", Font::Title) - 8), Theme::TEXT_LABEL);
        
        if (data.recent.empty()) {
            fb.drawText(x, listY, data.state == MqttData::State::Online ? "Waiting for messages" : "No connection",
                        Theme::TEXT_FAINT);
            return;
        }
        
        // Newest at the bottom; older lines move up and off the top
        int lines = std::max(1, (r.bottom() - 3 - listY + (LINE_PITCH - fb.textHeight())) / LINE_PITCH);
        int count = static_cast<int>(data.recent.size());
        int first = std::max(0, count - lines);
        
        for (int i = first; i < count; i++) {
            const MqttMessage& m = data.recent[i];
            int y = listY + (i - first) * LINE_PITCH;
            
            // Topic on the left, as much of the payload as fits after it
            std::string topic = fb.fitTextMiddle(m.topic, innerW * TOPIC_PCT / 100);
            int topicW = fb.textWidth(topic);
            fb.drawText(x, y, topic, m.retained ? Theme::VALUE_ALT : Theme::TITLE);
            
            int payloadX = x + topicW + 6;
            std::string payload = fb.fitText(m.payload.empty() ? "(empty)" : m.payload, x + innerW - payloadX);
            fb.drawText(payloadX, y, payload, m.payload.empty() ? Theme::TEXT_FAINT : Theme::TEXT);
        }
    }
};
