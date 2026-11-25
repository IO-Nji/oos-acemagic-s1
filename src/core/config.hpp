#pragma once
#include <string>
#include <vector>

// Settings read from the config file. Every field has a working default, so the file
// is optional.
struct Config {
    int pageIntervalSec = 5;
    std::vector<std::string> pages = {"system", "network", "sensors", "docker", "services", "identity"};
    std::vector<std::string> services = {"ssh", "docker", "oos-acemagic-s1", "nginx", "apache2",
                                         "mysql", "mariadb", "postgresql", "redis"};
    std::string diskPath = "/";
    
    static constexpr const char* DEFAULT_PATH = "/etc/oos-acemagic-s1/config.conf";
    
    // Apply "key = value" lines from text. Problems are appended to warnings and the
    // affected setting keeps its previous value.
    void apply(const std::string& text, std::vector<std::string>& warnings);
    
    // Load a file. Returns false if it cannot be read.
    bool load(const std::string& path, std::vector<std::string>& warnings);
};
