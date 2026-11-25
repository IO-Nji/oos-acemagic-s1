// Renders every dashboard page to PNG without touching the USB panel.
//   oos-s1-preview --mock typical|stress [--scale N] --out DIR   fixed data, repeatable
//   oos-s1-preview --live [--config FILE] --out DIR              data from this machine
//   oos-s1-preview --check                                       layout and contrast checks
#include "core/config.hpp"
#include "core/framebuffer.hpp"
#include "hw/protocol.hpp"
#include "ui/pages.hpp"

#include "mock_data.hpp"
#include "png_writer.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <thread>

// The dashboard exactly as the service builds it, minus the panel
struct Rig {
    Config config;
    Providers providers;
    Framebuffer fb{protocol::DISPLAY_WIDTH, protocol::DISPLAY_HEIGHT};
    std::shared_ptr<Health> health = std::make_shared<Health>();
    ScreenManager screens{fb, health};

    explicit Rig(const Config& c = Config()) : config(c), providers(c) {
        addPages(screens, config, providers, health);
        screens.setPageInterval(std::chrono::milliseconds(0));
    }

    void setData(const mock::Set& s) {
        providers.system->setData(s.system);
        providers.network->setData(s.network);
        providers.docker->setData(s.docker);
        providers.services->setData(s.services);
        providers.identity->setData(s.identity);
        providers.sensors->setData(s.sensors);
    }

    // Feed a waveform through the history graphs, ending on the base values
    void loadMock(const mock::Set& base) {
        screens.setTimeOverride(base.identity.currentTime);
        const int steps = 48;
        for (int i = 0; i <= steps; i++) {
            mock::Set s = base;
            if (i < steps) {
                float w = mock::wave(i), w2 = mock::wave(i, 2.0f);
                s.system.cpu.usagePercent = std::min(100.0f, base.system.cpu.usagePercent * (0.4f + 1.2f * w));
                s.sensors.cpuTempC = base.sensors.cpuTempC * (0.85f + 0.3f * w2);
                s.sensors.gpuTempC = base.sensors.gpuTempC * (0.85f + 0.3f * w);
                s.sensors.ssdTempC = base.sensors.ssdTempC * (0.9f + 0.2f * w);
                for (auto& n : s.network.interfaces) {
                    n.rxBps = static_cast<uint64_t>(n.rxBps * (0.2f + 1.6f * w));
                    n.txBps = static_cast<uint64_t>(n.txBps * (0.2f + 1.6f * w2));
                }
                mock::finish(s);
            }
            setData(s);
            screens.update();
        }
    }

    void collectLive() {
        for (int i = 0; i < 4; i++) {
            providers.system->update();
            providers.network->update();
            providers.sensors->update();
            providers.identity->update();
            if (i == 0) {
                providers.docker->update();
                providers.services->update();
            }
            screens.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    void renderPage(size_t i) {
        *health = health::evaluate(providers.system->data(), providers.sensors->data(),
                                   providers.docker->data(), providers.services->data());
        screens.setPage(i);
        screens.render();
    }
};

// Panel colour (high byte first) -> 8-bit channels
static void toRgb888(uint16_t panel, uint8_t* rgb) {
    uint16_t v = swapEndian16(panel);
    uint8_t r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
    rgb[0] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
    rgb[1] = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
    rgb[2] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
}

// Framebuffer -> RGB, each pixel repeated scale times in both directions
static std::vector<uint8_t> toRgb(const Framebuffer& fb, int scale) {
    int w = fb.width() * scale, h = fb.height() * scale;
    std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            toRgb888(fb.pixels()[(y / scale) * fb.width() + x / scale],
                     &rgb[(static_cast<size_t>(y) * w + x) * 3]);
        }
    }
    return rgb;
}

// --- checks -----------------------------------------------------------------

static double luminance(uint16_t panel) {
    uint8_t c[3];
    toRgb888(panel, c);
    double lin[3];
    for (int i = 0; i < 3; i++) {
        double v = c[i] / 255.0;
        lin[i] = v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    }
    return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2];
}

static double contrast(uint16_t a, uint16_t b) {
    double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// Every text colour must reach 4.5:1 on the surface it is drawn on, measured on the
// colours the panel actually shows (after RGB565 rounding).
static int checkContrast() {
    struct Pair { const char* name; uint16_t fg; };
    const Pair text[] = {
        {"TEXT", Theme::TEXT}, {"TEXT_LABEL", Theme::TEXT_LABEL}, {"TEXT_FAINT", Theme::TEXT_FAINT},
        {"TITLE", Theme::TITLE}, {"VALUE", Theme::VALUE}, {"VALUE_DIM", Theme::VALUE_DIM},
        {"VALUE_ALT", Theme::VALUE_ALT}, {"CLOCK", Theme::CLOCK}, {"OK", Theme::OK},
        {"WARN", Theme::WARN}, {"CRIT", Theme::CRIT}, {"INFO", Theme::INFO}, {"OFF", Theme::OFF},
        {"LEVEL_OK", Theme::LEVEL_OK}, {"LEVEL_WARN", Theme::LEVEL_WARN}, {"LEVEL_CRIT", Theme::LEVEL_CRIT},
        {"DOWN", Theme::DOWN}, {"UP", Theme::UP},
    };
    int failures = 0;
    double lowest = 99.0;
    for (const auto& p : text) {
        // Cards, header and footer use SURFACE, the lightest background text sits on
        double ratio = contrast(p.fg, Theme::SURFACE);
        lowest = std::min(lowest, ratio);
        if (ratio < 4.5) {
            std::cerr << "contrast: " << p.name << " on SURFACE is " << ratio << ":1 (needs 4.5)\n";
            failures++;
        }
    }
    std::cout << "contrast: " << (sizeof(text) / sizeof(text[0])) << " text colours, lowest "
              << lowest << ":1\n";
    return failures;
}

// No text may leave the frame or collide with other text, on either data set.
static int checkLayout() {
    int failures = 0, boxes = 0, renders = 0;
    const int minX = Layout::BORDER_X + 1, minY = Layout::BORDER_Y + 1;
    const int maxX = Layout::BORDER_X + Layout::BORDER_W - 1;
    const int maxY = Layout::BORDER_Y + Layout::BORDER_H - 1;

    for (const char* setName : {"typical", "stress"}) {
        Rig rig;
        rig.loadMock(std::strcmp(setName, "stress") == 0 ? mock::stress() : mock::typical());
        for (size_t page = 0; page < rig.screens.pageCount(); page++) {
            rig.fb.setAudit(true);
            rig.renderPage(page);
            renders++;
            const auto& tb = rig.fb.textBoxes();
            boxes += static_cast<int>(tb.size());
            std::string where = std::string(setName) + "/" + rig.config.pages[page];
            for (size_t i = 0; i < tb.size(); i++) {
                const auto& a = tb[i];
                if (a.x < minX || a.y < minY || a.x + a.w > maxX || a.y + a.h > maxY) {
                    std::cerr << "layout: " << where << " \"" << a.text << "\" leaves the frame at "
                              << a.x << "," << a.y << "\n";
                    failures++;
                }
                for (size_t j = i + 1; j < tb.size(); j++) {
                    const auto& b = tb[j];
                    if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h) {
                        std::cerr << "layout: " << where << " \"" << a.text << "\" overlaps \""
                                  << b.text << "\"\n";
                        failures++;
                    }
                }
            }
            rig.fb.setAudit(false);
        }
    }
    std::cout << "layout: " << boxes << " text boxes on " << renders << " renders checked\n";
    return failures;
}

// --- main -------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::string mode = "typical", outDir = ".", configPath;
    bool live = false, check = false;
    int scale = 1;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--mock" && i + 1 < argc) mode = argv[++i];
        else if (arg == "--live") live = true;
        else if (arg == "--check") check = true;
        else if (arg == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (arg == "--config" && i + 1 < argc) configPath = argv[++i];
        else if (arg == "--scale" && i + 1 < argc) scale = std::max(1, std::min(8, std::atoi(argv[++i])));
        else {
            std::cerr << "Usage: " << argv[0] << " [--mock typical|stress | --live [--config FILE]]"
                         " [--scale N] [--out DIR] | --check\n";
            return 2;
        }
    }

    if (check) {
        int failures = checkContrast() + checkLayout();
        if (failures) std::cerr << failures << " check(s) failed\n";
        return failures ? 1 : 0;
    }

    if (!live && mode != "typical" && mode != "stress") {
        std::cerr << "Unknown mock set: " << mode << "\n";
        return 2;
    }

    Config config;
    if (!configPath.empty()) {
        std::vector<std::string> warnings;
        if (!config.load(configPath, warnings)) {
            std::cerr << "Cannot read " << configPath << "\n";
            return 2;
        }
        for (const auto& w : warnings) std::cerr << configPath << ": " << w << "\n";
    }

    Rig rig(config);
    if (live) rig.collectLive();
    else rig.loadMock(mode == "stress" ? mock::stress() : mock::typical());

    for (size_t i = 0; i < rig.screens.pageCount(); i++) {
        rig.renderPage(i);
        std::string path = outDir + "/" + std::to_string(i + 1) + "_" + rig.config.pages[i] + ".png";
        if (!png::write(path, rig.fb.width() * scale, rig.fb.height() * scale, toRgb(rig.fb, scale))) {
            std::cerr << "Cannot write " << path << "\n";
            return 1;
        }
        std::cout << path << "\n";
    }
    return 0;
}
