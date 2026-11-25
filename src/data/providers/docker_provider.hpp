#pragma once
#include "data/provider_base.hpp"
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Container list and resource use through the docker CLI: one "docker ps" and one
// "docker stats" call per refresh, whatever the number of containers.
class DockerDataProvider : public SnapshotProvider<DockerData> {
public:
    std::string name() const override { return "Docker"; }
    
    // "Up 6 weeks (healthy)" -> seconds
    static int parseUptime(const std::string& status) {
        if (status.compare(0, 3, "Up ") != 0) return 0;
        std::istringstream ss(status.substr(3));
        std::string first, word;
        ss >> first;
        
        int value = 0;
        if (first == "About") {          // "About a minute", "About an hour"
            ss >> word >> word;
            value = 1;
        } else if (first == "Less") {    // "Less than a second"
            return 0;
        } else {
            value = std::atoi(first.c_str());
            ss >> word;
        }
        
        if (word.find("second") == 0) return value;
        if (word.find("minute") == 0) return value * 60;
        if (word.find("hour") == 0) return value * 3600;
        if (word.find("day") == 0) return value * 86400;
        if (word.find("week") == 0) return value * 7 * 86400;
        if (word.find("month") == 0) return value * 30 * 86400;
        if (word.find("year") == 0) return value * 365 * 86400;
        return 0;
    }
    
    // "212.4MiB / 15.4GiB" -> 212 (MB in use)
    static int parseMemoryMB(const std::string& usage) {
        double value = 0;
        char unit[8] = "";
        if (sscanf(usage.c_str(), "%lf%7[A-Za-z]", &value, unit) < 1) return 0;
        std::string u(unit);
        if (u == "GiB" || u == "GB") value *= 1024;
        else if (u == "KiB" || u == "kB" || u == "KB") value /= 1024;
        else if (u == "B") value /= 1024 * 1024;
        return static_cast<int>(value + 0.5);
    }
    
    // Build the container list from "docker ps" lines (ID|Names|Image|Status|State)
    // and optional "docker stats" lines (ID|CPUPerc|MemUsage)
    static DockerData parse(const std::string& psOutput, const std::string& statsOutput) {
        DockerData d;
        d.available = true;
        
        std::map<std::string, std::pair<float, int>> stats;
        std::istringstream statsStream(statsOutput);
        std::string line;
        while (std::getline(statsStream, line)) {
            std::vector<std::string> f = split(line, '|');
            if (f.size() < 3) continue;
            stats[f[0].substr(0, 12)] = {static_cast<float>(std::atof(f[1].c_str())), parseMemoryMB(f[2])};
        }
        
        std::istringstream psStream(psOutput);
        while (std::getline(psStream, line)) {
            std::vector<std::string> f = split(line, '|');
            if (f.size() < 5) continue;
            
            DockerContainerData c;
            c.id = f[0].substr(0, 12);
            c.name = f[1];
            c.image = f[2];
            c.uptimeSeconds = parseUptime(f[3]);
            c.status = f[4];
            c.unhealthy = f[3].find("(unhealthy)") != std::string::npos;
            
            auto it = stats.find(c.id);
            if (it != stats.end()) {
                c.cpuPercent = it->second.first;
                c.memUsageMB = it->second.second;
            }
            
            if (c.status == "running") d.running++;
            else if (c.status == "paused") d.paused++;
            else d.stopped++;
            if (needsAttention(c)) d.unhealthy++;
            
            d.containers.push_back(c);
        }
        d.total = static_cast<int>(d.containers.size());
        
        // The page shows only the first few: put trouble first, then the busiest
        std::stable_sort(d.containers.begin(), d.containers.end(),
                         [](const DockerContainerData& a, const DockerContainerData& b) {
                             int ra = rank(a), rb = rank(b);
                             if (ra != rb) return ra < rb;
                             return a.cpuPercent > b.cpuPercent;
                         });
        return d;
    }
    
    static bool needsAttention(const DockerContainerData& c) {
        return c.unhealthy || c.status == "restarting" || c.status == "dead";
    }
    
protected:
    DockerData collect() override {
        // The marker distinguishes "no containers" from "docker unavailable"
        std::string ps = runCommand(
            "docker ps -a --format '{{.ID}}|{{.Names}}|{{.Image}}|{{.Status}}|{{.State}}' 2>/dev/null"
            " && echo __OK__");
        if (ps.find("__OK__") == std::string::npos) return DockerData();
        
        std::string stats;
        if (ps.find("|running") != std::string::npos) {
            stats = runCommand(
                "docker stats --no-stream --format '{{.ID}}|{{.CPUPerc}}|{{.MemUsage}}' 2>/dev/null", 20);
        }
        return parse(ps, stats);
    }
    
private:
    static int rank(const DockerContainerData& c) {
        if (needsAttention(c)) return 0;
        if (c.status == "running") return 1;
        if (c.status == "paused") return 2;
        return 3;
    }
    
    static std::vector<std::string> split(const std::string& line, char sep) {
        std::vector<std::string> fields;
        std::istringstream ss(line);
        std::string token;
        while (std::getline(ss, token, sep)) fields.push_back(token);
        return fields;
    }
};
