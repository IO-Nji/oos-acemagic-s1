#include "core/config.hpp"
#include "core/framebuffer.hpp"
#include "data/collector.hpp"
#include "data/health.hpp"
#include "hw/panel.hpp"
#include "ui/pages.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <iostream>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

#ifndef OOS_VERSION
#define OOS_VERSION "dev"
#endif

static std::atomic<bool> running{true};

static void signalHandler(int) {
    running = false;
}

static std::tm localNow() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    return tm;
}

static void reportTransfers(long sent, long failed) {
    std::cout << "panel: " << sent << " updates sent, " << failed << " timed out" << std::endl;
}

static void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "  --config <file>   Settings file (default: " << Config::DEFAULT_PATH << ")\n"
              << "  --interval <sec>  Seconds per page, overriding the settings file\n"
              << "  --dry-run         Collect and render without opening the panel\n"
              << "  --version         Print the version\n"
              << "  --help            Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string configPath = Config::DEFAULT_PATH;
    bool configGiven = false, dryRun = false;
    int intervalOverride = 0;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
            configGiven = true;
        } else if (arg == "--interval" && i + 1 < argc) {
            intervalOverride = std::atoi(argv[++i]);
        } else if (arg == "--dry-run") {
            dryRun = true;
        } else if (arg == "--version") {
            std::cout << "oos-acemagic-s1 " << OOS_VERSION << std::endl;
            return 0;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 2;
        }
    }
    
    Config config;
    std::vector<std::string> warnings;
    if (!config.load(configPath, warnings) && configGiven) {
        std::cerr << "Cannot read " << configPath << std::endl;
        return 2;
    }
    for (const auto& w : warnings) {
        std::cerr << configPath << ": " << w << std::endl;
    }
    if (intervalOverride > 0) config.pageIntervalSec = intervalOverride;
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    Panel panel;
    if (!dryRun && !panel.open()) {
        std::cerr << "Cannot open the S1 panel" << std::endl;
        return 1;
    }
    
    Providers providers(config);
    
    // Data is collected off the render thread. Cheap /proc and /sys readers share one
    // thread; sources that start subprocesses get their own so they cannot delay it.
    Collector fastCollector, slowCollector;
    fastCollector.add(providers.system, 1s);
    fastCollector.add(providers.network, 1s);
    fastCollector.add(providers.sensors, 2s);
    fastCollector.add(providers.identity, 5s);
    slowCollector.add(providers.docker, 10s);
    slowCollector.add(providers.services, 10s);
    fastCollector.start();
    slowCollector.start();
    
    Framebuffer fb(protocol::DISPLAY_WIDTH, protocol::DISPLAY_HEIGHT);
    auto health = std::make_shared<Health>();
    ScreenManager screens(fb, health);
    addPages(screens, config, providers, health);
    screens.setPageInterval(std::chrono::seconds(config.pageIntervalSec));
    
    std::cout << "oos-acemagic-s1 " << OOS_VERSION << ": " << screens.pageCount() << " pages, "
              << config.pageIntervalSec << "s each" << (dryRun ? " (dry run)" : "") << std::endl;
    
    // Keepalive on its own thread so a long repaint never delays it
    std::thread heartbeatThread([&panel, dryRun]() {
        auto lastSetTime = std::chrono::steady_clock::now();
        while (!dryRun && running.load()) {
            std::tm now = localNow();
            panel.sendHeartbeat(now);
            
            if (std::chrono::steady_clock::now() - lastSetTime >= 60s) {
                panel.setTime(now);
                lastSetTime = std::chrono::steady_clock::now();
            }
            
            // Short naps so shutdown is not held up for a full second
            for (int i = 0; i < 10 && running.load(); i++) {
                std::this_thread::sleep_for(100ms);
            }
        }
    });
    
    const auto frameInterval = 250ms;
    const auto reportInterval = 10min;   // also reported at shutdown
    auto reportStart = std::chrono::steady_clock::now();
    long tilesSent = 0, tilesFailed = 0;
    int exitCode = 0;
    
    while (running) {
        auto frameStart = std::chrono::steady_clock::now();
        
        *health = health::evaluate(providers.system->data(), providers.sensors->data(),
                                   providers.docker->data(), providers.services->data());
        screens.update();
        screens.render();
        
        if (!dryRun) {
            Panel::PresentResult result = panel.present(fb);
            tilesSent += result.sent;
            tilesFailed += result.failed;
            
            if (panel.disconnected()) {
                std::cerr << "Panel disconnected" << std::endl;
                exitCode = 1;   // the service manager restarts us; the panel may come back
                break;
            }
        }
        
        // Transfer statistics, so a struggling link shows up in the journal
        if (frameStart - reportStart >= reportInterval) {
            reportTransfers(tilesSent, tilesFailed);
            reportStart = frameStart;
            tilesSent = tilesFailed = 0;
        }
        
        std::this_thread::sleep_until(frameStart + frameInterval);
    }
    
    if (!dryRun) reportTransfers(tilesSent, tilesFailed);
    running = false;
    heartbeatThread.join();
    fastCollector.stop();
    slowCollector.stop();
    if (!dryRun) panel.close();
    
    return exitCode;
}
