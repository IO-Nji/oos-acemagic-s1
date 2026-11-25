#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

// Settings read from the config file. Every field has a working default, so the file
// is optional.
struct Config {
    int pageIntervalSec = 5;
    std::vector<std::string> pages = {"system", "network", "sensors", "docker", "services", "identity"};
    std::vector<std::string> services = {"ssh", "docker", "nginx", "apache2", "mysql", "mariadb",
                                         "postgresql", "redis", "mosquitto"};
    std::string diskPath = "/";
    
    // MQTT page: the broker to watch and the topic filters whose messages are listed
    std::string mqttHost = "127.0.0.1";
    int mqttPort = 1883;
    std::string mqttUser, mqttPassword;
    std::vector<std::string> mqttTopics = {"#"};
    
    // Settings that belong to one device rather than to the dashboard (a display
    // device path, for example). Name them in deviceKeys and they are collected here
    // as text for the device code to interpret.
    std::set<std::string> deviceKeys;
    std::map<std::string, std::string> deviceSettings;
    
    // Defaults for a program whose own systemd unit is ownService; the unit joins
    // the watch list
    static Config defaultsFor(const std::string& ownService) {
        Config c;
        c.services.push_back(ownService);
        return c;
    }
    
    // Apply "key = value" lines from text. Problems are appended to warnings and the
    // affected setting keeps its previous value.
    void apply(const std::string& text, std::vector<std::string>& warnings);
    
    // Load a file. Returns false if it cannot be read.
    bool load(const std::string& path, std::vector<std::string>& warnings);
};
