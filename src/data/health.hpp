#pragma once
#include "data/data_types.hpp"
#include <cstdio>
#include <string>
#include <vector>

// Overall machine state, shown in the footer of every page and on the identity page.
struct Health {
    enum class Level { Ok, Warning, Critical };
    
    Level level = Level::Ok;
    std::vector<std::string> alerts;   // most severe first, short enough for the footer
    
    // "OK", or the first alert with a count of the others
    std::string summary() const {
        if (alerts.empty()) return "OK";
        if (alerts.size() == 1) return alerts[0];
        return alerts[0] + " +" + std::to_string(alerts.size() - 1);
    }
};

namespace health {
    constexpr float DISK_WARN = 90.0f, DISK_CRIT = 97.0f;
    constexpr float MEM_WARN = 92.0f;
    constexpr float TEMP_WARN = 85.0f, TEMP_CRIT = 95.0f;
    
    inline std::string percentAlert(const char* what, float value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s %d%%", what, static_cast<int>(value));
        return buf;
    }
    
    // mqttOffline: the MQTT page is in use and its broker cannot be reached
    inline Health evaluate(const SystemData& system, const SensorsData& sensors,
                           const DockerData& docker, const ServicesData& services,
                           bool mqttOffline = false) {
        std::vector<std::string> critical, warning;
        
        if (mqttOffline) {
            critical.push_back("MQTT OFFLINE");
        }
        if (services.failedCount > 0) {
            critical.push_back(std::to_string(services.failedCount) + " SVC FAILED");
        }
        if (system.disk.usagePercent >= DISK_CRIT) {
            critical.push_back(percentAlert("DISK", system.disk.usagePercent));
        } else if (system.disk.usagePercent >= DISK_WARN) {
            warning.push_back(percentAlert("DISK", system.disk.usagePercent));
        }
        if (sensors.cpuTempC >= TEMP_WARN) {
            char buf[32];
            snprintf(buf, sizeof(buf), "CPU %dC", static_cast<int>(sensors.cpuTempC));
            (sensors.cpuTempC >= TEMP_CRIT ? critical : warning).push_back(buf);
        }
        if (sensors.underVoltage) {
            warning.push_back("LOW VOLTAGE");
        }
        if (docker.unhealthy > 0) {
            warning.push_back(std::to_string(docker.unhealthy) + " CTR UNHEALTHY");
        }
        if (system.memory.usagePercent >= MEM_WARN) {
            warning.push_back(percentAlert("MEM", system.memory.usagePercent));
        }
        
        Health h;
        h.level = !critical.empty() ? Health::Level::Critical
                : !warning.empty()  ? Health::Level::Warning
                                    : Health::Level::Ok;
        h.alerts = critical;
        h.alerts.insert(h.alerts.end(), warning.begin(), warning.end());
        return h;
    }
}
