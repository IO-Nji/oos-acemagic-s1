#include "core/config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// A comment starts with '#' at the beginning of the line or standing alone between
// spaces. A '#' inside a value, such as the MQTT wildcard in "sensors/#" or a lone
// "#" at the end of the line, is kept.
static std::string stripComment(const std::string& line) {
    size_t first = line.find_first_not_of(" \t");
    if (first != std::string::npos && line[first] == '#') return "";
    for (size_t i = 1; i + 1 < line.size(); i++) {
        if (line[i] == '#' && std::isspace(static_cast<unsigned char>(line[i - 1])) &&
            std::isspace(static_cast<unsigned char>(line[i + 1]))) {
            return line.substr(0, i);
        }
    }
    return line;
}

static std::vector<std::string> splitList(const std::string& value) {
    std::vector<std::string> items;
    std::istringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) items.push_back(item);
    }
    return items;
}

void Config::apply(const std::string& text, std::vector<std::string>& warnings) {
    static const std::vector<std::string> knownPages = {"system", "network", "sensors", "docker",
                                                        "services", "mqtt", "identity"};
    std::istringstream stream(text);
    std::string line;
    int lineNo = 0;
    
    while (std::getline(stream, line)) {
        lineNo++;
        line = trim(stripComment(line));
        if (line.empty()) continue;
        
        std::string where = "line " + std::to_string(lineNo) + ": ";
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            warnings.push_back(where + "expected key = value");
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        
        if (key == "page_interval") {
            char* end = nullptr;
            long n = std::strtol(value.c_str(), &end, 10);
            if (value.empty() || *end != '\0' || n < 2 || n > 3600) {
                warnings.push_back(where + "page_interval must be 2 to 3600 seconds");
            } else {
                pageIntervalSec = static_cast<int>(n);
            }
        } else if (key == "pages") {
            std::vector<std::string> list;
            for (const auto& p : splitList(value)) {
                if (std::find(knownPages.begin(), knownPages.end(), p) == knownPages.end()) {
                    warnings.push_back(where + "unknown page \"" + p + "\"");
                } else if (std::find(list.begin(), list.end(), p) == list.end()) {
                    list.push_back(p);
                }
            }
            if (list.empty()) {
                warnings.push_back(where + "pages needs at least one valid page");
            } else {
                pages = list;
            }
        } else if (key == "services") {
            services = splitList(value);
        } else if (key == "disk") {
            if (value.empty() || value[0] != '/') {
                warnings.push_back(where + "disk must be an absolute path");
            } else {
                diskPath = value;
            }
        } else if (key == "mqtt_host") {
            if (value.empty()) warnings.push_back(where + "mqtt_host needs a host name or address");
            else mqttHost = value;
        } else if (key == "mqtt_port") {
            char* end = nullptr;
            long n = std::strtol(value.c_str(), &end, 10);
            if (value.empty() || *end != '\0' || n < 1 || n > 65535) {
                warnings.push_back(where + "mqtt_port must be 1 to 65535");
            } else {
                mqttPort = static_cast<int>(n);
            }
        } else if (key == "mqtt_user") {
            mqttUser = value;
        } else if (key == "mqtt_password") {
            mqttPassword = value;
        } else if (key == "mqtt_topics") {
            std::vector<std::string> list = splitList(value);
            if (list.empty()) warnings.push_back(where + "mqtt_topics needs at least one topic filter");
            else mqttTopics = list;
        } else if (deviceKeys.count(key)) {
            deviceSettings[key] = value;
        } else {
            warnings.push_back(where + "unknown setting \"" + key + "\"");
        }
    }
}

bool Config::load(const std::string& path, std::vector<std::string>& warnings) {
    std::ifstream file(path);
    if (!file) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    apply(buffer.str(), warnings);
    return true;
}
