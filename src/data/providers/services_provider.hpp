#pragma once
#include "data/provider_base.hpp"
#include <set>
#include <sstream>
#include <string>
#include <vector>

// systemd unit states, read with a single "systemctl show" call per refresh. Units
// that do not exist on this machine are left out, and aliases of one unit are
// listed once.
class ServicesDataProvider : public SnapshotProvider<ServicesData> {
public:
    explicit ServicesDataProvider(std::vector<std::string> services)
        : watched_(std::move(services)) {}
    
    std::string name() const override { return "Services"; }
    
    // Parse "systemctl show" output: one Key=Value block per unit, blank line between
    static ServicesData parseShow(const std::string& out) {
        ServicesData d;
        std::set<std::string> seen;
        std::istringstream stream(out);
        std::string line, id, loadState, activeState;
        
        auto flush = [&] {
            if (!id.empty() && loadState != "not-found" && seen.insert(id).second) {
                const std::string suffix = ".service";
                std::string shortName = id;
                if (id.size() > suffix.size() &&
                    id.compare(id.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    shortName.erase(id.size() - suffix.size());
                }
                if (activeState == "active") d.activeCount++;
                else if (activeState == "failed") d.failedCount++;
                d.services.push_back({shortName, activeState});
            }
            id.clear();
            loadState.clear();
            activeState.clear();
        };
        
        while (std::getline(stream, line)) {
            if (line.empty()) {
                flush();
                continue;
            }
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq), value = line.substr(eq + 1);
            
            if (key == "Id") id = value;
            else if (key == "LoadState") loadState = value;
            else if (key == "ActiveState") activeState = value;
        }
        flush();
        return d;
    }
    
    // Unit names go on a shell command line, so accept only what systemd itself allows
    static bool isSafeUnitName(const std::string& s) {
        if (s.empty() || s[0] == '-') return false;
        for (char c : s) {
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '_' || c == '.' || c == '@' || c == ':';
            if (!ok) return false;
        }
        return true;
    }
    
protected:
    ServicesData collect() override {
        std::string units;
        for (const auto& s : watched_) {
            if (isSafeUnitName(s)) units += " " + s;
        }
        if (units.empty()) return ServicesData();
        
        return parseShow(runCommand(
            "systemctl show --no-pager --property=Id,LoadState,ActiveState" + units + " 2>/dev/null"));
    }
    
private:
    std::vector<std::string> watched_;
};
