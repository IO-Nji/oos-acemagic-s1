#pragma once
#include "core/config.hpp"
#include "core/framebuffer.hpp"
#include "core/layout.hpp"
#include "data/collector.hpp"
#include "data/health.hpp"
#include "ui/pages.hpp"
#include <chrono>
#include <memory>

// The drawing surface of a device, in layout units. The dashboard frame occupies the
// top-left 320x170 units; a device with a taller surface uses the rest itself.
struct DisplayProfile {
    int width = Layout::SCREEN_WIDTH;
    int height = Layout::SCREEN_HEIGHT;
    UiScale scale;
};

// Everything a device program needs apart from its hardware: data providers,
// background collection, health evaluation and the pages, drawn into a framebuffer.
class Dashboard {
public:
    Dashboard(const Config& config, const DisplayProfile& display)
        : providers_(config),
          fb_(display.width, display.height, display.scale),
          health_(std::make_shared<Health>()),
          screens_(fb_, health_) {
        addPages(screens_, config, providers_, health_);
        screens_.setPageInterval(std::chrono::seconds(config.pageIntervalSec));
    }
    
    // Data is collected off the render thread. Cheap /proc and /sys readers share one
    // thread; sources that start subprocesses get their own so they cannot delay it.
    void startCollectors() {
        using namespace std::chrono_literals;
        fast_.add(providers_.system, 1s);
        fast_.add(providers_.network, 1s);
        fast_.add(providers_.sensors, 2s);
        fast_.add(providers_.identity, 5s);
        slow_.add(providers_.docker, 10s);
        slow_.add(providers_.services, 10s);
        fast_.start();
        slow_.start();
    }
    
    void stopCollectors() {
        fast_.stop();
        slow_.stop();
    }
    
    // Draw the current page from the latest data
    void renderFrame() {
        *health_ = health::evaluate(providers_.system->data(), providers_.sensors->data(),
                                    providers_.docker->data(), providers_.services->data());
        screens_.update();
        screens_.render();
    }
    
    Framebuffer& framebuffer() { return fb_; }
    ScreenManager& screens() { return screens_; }
    Providers& providers() { return providers_; }
    const Health& health() const { return *health_; }
    
private:
    Providers providers_;
    Framebuffer fb_;
    std::shared_ptr<Health> health_;
    ScreenManager screens_;
    Collector fast_, slow_;
};
