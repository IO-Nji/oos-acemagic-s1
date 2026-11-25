#pragma once
#include "ui/screen_manager.hpp"
#include "data/providers/system_provider.hpp"
#include "data/providers/sensors_provider.hpp"
#include <memory>
#include <vector>

//=============================================================================
// SystemScreen - CPU, memory, disk, power and uptime
// Row 1: CPU and memory cards with ring gauges
// Row 2: disk, power and uptime cards
//=============================================================================
class SystemScreen : public ScreenBase {
private:
    static constexpr int ROW1_H = 60;
    static constexpr float FALLBACK_RATED_W = 15.0f;   // when the CPU does not report one
    
public:
    SystemScreen(std::shared_ptr<SystemDataProvider> provider,
                 std::shared_ptr<SensorsDataProvider> sensorsProvider)
        : ScreenBase("SYSTEM"), provider_(std::move(provider)),
          sensorsProvider_(std::move(sensorsProvider)) {
        cpuHistory_.resize(HISTORY_SIZE, 0);
    }
    
    // Add each newly collected sample to the history graph
    void update() override {
        if (provider_->generation() == seenGeneration_) return;
        seenGeneration_ = provider_->generation();
        
        cpuHistory_.erase(cpuHistory_.begin());
        cpuHistory_.push_back(provider_->data().cpu.usagePercent);
    }
    
    void render(Framebuffer& fb, const Rect& area) override {
        const auto data = provider_->data();
        
        Rect row1(area.x, area.y, area.w, ROW1_H);
        Rect row2(area.x, area.y + ROW1_H + CARD_GAP, area.w, area.h - ROW1_H - CARD_GAP);
        
        renderCpuCard(fb, row1.column(0, 2, CARD_GAP), data);
        renderMemoryCard(fb, row1.column(1, 2, CARD_GAP), data.memory);
        
        renderDiskCard(fb, row2.column(0, 3, CARD_GAP), data);
        renderPowerCard(fb, row2.column(1, 3, CARD_GAP));
        renderUptimeCard(fb, row2.column(2, 3, CARD_GAP), data);
    }
    
private:
    std::shared_ptr<SystemDataProvider> provider_;
    std::shared_ptr<SensorsDataProvider> sensorsProvider_;
    
    static constexpr int HISTORY_SIZE = 20;
    std::vector<float> cpuHistory_;
    uint64_t seenGeneration_ = 0;
    
    // Vertical positions inside a row-1 card
    static constexpr int R1_TITLE_Y = 5;
    static constexpr int R1_GAUGE_CY = 34;
    static constexpr int R1_STAT_Y = 20;     // first of three stat lines
    static constexpr int R1_STAT_PITCH = 11;
    static constexpr int R1_STRIP_Y = 53;    // sparkline / breakdown bar
    static constexpr int R1_STRIP_H = 5;
    
    // Vertical positions inside a row-2 card
    static constexpr int R2_TITLE_Y = 5;
    static constexpr int R2_VALUE_Y = 17;
    static constexpr int R2_DETAIL_Y = 33;
    static constexpr int R2_BAR_Y = 46;
    
    void renderCpuCard(Framebuffer& fb, const Rect& r, const SystemData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        drawSectionTitle(fb, x, r.y + R1_TITLE_Y, "CPU", Theme::SEC_CPU);
        
        char buf[32];
        uint16_t cpuColor = percentToColor(data.cpu.usagePercent);
        snprintf(buf, sizeof(buf), "%d", static_cast<int>(data.cpu.usagePercent));
        drawGaugeRing(fb, x + GAUGE_R, r.y + R1_GAUGE_CY, data.cpu.usagePercent, buf, "%", cpuColor);
        
        int statsX = x + GAUGE_R * 2 + 8;
        int statsW = r.right() - CARD_PAD - statsX;
        int statsY = r.y + R1_STAT_Y;
        
        snprintf(buf, sizeof(buf), "%.1fGHz", data.cpu.freqMHz / 1000.0f);
        fb.drawText(statsX, statsY, fb.fitText(buf, statsW), Theme::VALUE);
        
        snprintf(buf, sizeof(buf), "L:%.2f", data.cpu.loadAvg1);
        fb.drawText(statsX, statsY + R1_STAT_PITCH, fb.fitText(buf, statsW), Theme::TEXT);
        
        float temp = sensorsProvider_->data().cpuTempC;
        if (temp > 0) {
            snprintf(buf, sizeof(buf), "%.0fC", temp);
            fb.drawText(statsX, statsY + 2 * R1_STAT_PITCH, buf, tempToColor(temp));
        }
        
        if (!cpuHistory_.empty()) {
            fb.drawSparkline(x, r.y + R1_STRIP_Y, innerW - 1, R1_STRIP_H,
                             cpuHistory_.data(), HISTORY_SIZE,
                             Theme::GRAPH_LINE, true, Theme::GRAPH_FILL);
        }
    }
    
    void renderMemoryCard(Framebuffer& fb, const Rect& r, const MemoryData& mem) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        drawSectionTitle(fb, x, r.y + R1_TITLE_Y, "MEM", Theme::SEC_MEM);
        
        char buf[32];
        uint16_t memColor = percentToColor(mem.usagePercent);
        snprintf(buf, sizeof(buf), "%d", static_cast<int>(mem.usagePercent));
        drawGaugeRing(fb, x + GAUGE_R, r.y + R1_GAUGE_CY, mem.usagePercent, buf, "%", memColor);
        
        int statsX = x + GAUGE_R * 2 + 8;
        int statsW = r.right() - CARD_PAD - statsX;
        int statsY = r.y + R1_STAT_Y;
        
        // Megabytes on small machines, where gigabytes would read "0.1/0.4G"
        if (mem.totalMB < 2048) {
            snprintf(buf, sizeof(buf), "%d/%dM", mem.usedMB, mem.totalMB);
        } else {
            snprintf(buf, sizeof(buf), "%.1f/%.1fG", mem.usedMB / 1024.0f, mem.totalMB / 1024.0f);
        }
        fb.drawText(statsX, statsY, fb.fitText(buf, statsW), Theme::VALUE);
        
        snprintf(buf, sizeof(buf), "C:%dM", mem.cachedMB);
        fb.drawText(statsX, statsY + R1_STAT_PITCH, fb.fitText(buf, statsW), Theme::TEXT);
        
        snprintf(buf, sizeof(buf), "S:%d%%", static_cast<int>(mem.swapUsagePercent));
        fb.drawText(statsX, statsY + 2 * R1_STAT_PITCH, buf, percentToColor(mem.swapUsagePercent));
        
        // Breakdown bar: in use (same figure as the ring), page cache, buffers, free.
        // Cache and buffers are reclaimable, so they sit on top of "in use".
        float used = std::min(100.0f, mem.usagePercent);
        float cached = (mem.totalMB > 0) ? (mem.cachedMB * 100.0f / mem.totalMB) : 0;
        float buffers = (mem.totalMB > 0) ? (mem.buffersMB * 100.0f / mem.totalMB) : 0;
        cached = std::min(cached, 100.0f - used);
        buffers = std::min(buffers, 100.0f - used - cached);
        float free = 100.0f - used - cached - buffers;
        float percents[] = {used, cached, buffers, free};
        uint16_t colors[] = {Theme::MEM_USED, Theme::MEM_CACHED, Theme::MEM_BUFFERS, Theme::TRACK};
        fb.drawStackedBar(x, r.y + R1_STRIP_Y - 1, innerW, R1_STRIP_H + 2,
                          percents, colors, 4, Theme::BAR_BORDER);
    }
    
    void renderDiskCard(Framebuffer& fb, const Rect& r, const SystemData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        drawSectionTitle(fb, x, r.y + R2_TITLE_Y, "DISK", Theme::SEC_DISK);
        
        const DiskData& disk = data.disk;
        float diskPercent = disk.usagePercent;
        uint16_t diskColor = percentToColor(diskPercent);
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(diskPercent));
        fb.drawText(x, r.y + R2_VALUE_Y, buf, diskColor, Font::Large);
        
        snprintf(buf, sizeof(buf), "%d/%dG", disk.usedGB, disk.totalGB);
        fb.drawText(x, r.y + R2_DETAIL_Y, fb.fitText(buf, innerW), Theme::TEXT_LABEL);
        
        drawLevelBar(fb, x, r.y + R2_BAR_Y, innerW, diskPercent, diskColor);
    }
    
    void renderPowerCard(Framebuffer& fb, const Rect& r) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        drawSectionTitle(fb, x, r.y + R2_TITLE_Y, "POWER", Theme::SEC_POWER);
        
        const auto sensors = sensorsProvider_->data();
        float power = sensors.totalPowerW;
        if (power <= 0) {
            // RAPL is missing, or not readable because the program is not root
            fb.drawText(x, r.y + R2_VALUE_Y + 2, "N/A", Theme::TEXT_LABEL);
            fb.drawText(x, r.y + R2_DETAIL_Y, "No sensor", Theme::TEXT_FAINT);
            return;
        }
        
        float rated = sensors.tdpW > 0 ? sensors.tdpW : FALLBACK_RATED_W;
        uint16_t powerColor = powerToColor(power, rated);
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fW", power);
        fb.drawText(x, r.y + R2_VALUE_Y, fb.fitText(buf, innerW, Font::Large), powerColor, Font::Large);
        drawLevelBar(fb, x, r.y + R2_BAR_Y, innerW, std::min(100.0f, power / rated * 100.0f), powerColor);
    }
    
    void renderUptimeCard(Framebuffer& fb, const Rect& r, const SystemData& data) {
        drawCard(fb, r);
        int x = r.x + CARD_PAD;
        int innerW = r.w - 2 * CARD_PAD;
        drawSectionTitle(fb, x, r.y + R2_TITLE_Y, "UPTIME", Theme::SEC_UPTIME);
        
        char big[32], detail[32] = "";
        int secs = data.uptimeSeconds;
        int days = secs / 86400;
        int hours = (secs % 86400) / 3600;
        int mins = (secs % 3600) / 60;
        
        if (days > 0) {
            snprintf(big, sizeof(big), "%dd", days);
            snprintf(detail, sizeof(detail), "%dh %dm", hours, mins);
        } else if (hours > 0) {
            snprintf(big, sizeof(big), "%dh", hours);
            snprintf(detail, sizeof(detail), "%dm", mins);
        } else {
            snprintf(big, sizeof(big), "%dm", mins);
        }
        fb.drawText(x, r.y + R2_VALUE_Y, fb.fitText(big, innerW, Font::Large),
                    Theme::VALUE, Font::Large);
        
        fb.drawText(x, r.y + R2_DETAIL_Y, detail, Theme::TEXT_LABEL);
        fb.drawText(x, r.y + R2_BAR_Y - 3, fb.fitText(data.hostname, innerW), Theme::VALUE_ALT);
    }
};
