#pragma once
#include "data/provider_base.hpp"
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/utsname.h>
#include <unistd.h>
#include <utmp.h>

// Host identity, clock, uptime and login sessions. No subprocesses.
class IdentityDataProvider : public SnapshotProvider<IdentityData> {
public:
    IdentityDataProvider() {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) static_.hostname = hostname;
        
        struct utsname uts;
        if (uname(&uts) == 0) {
            static_.kernelVersion = uts.release;
            static_.architecture = uts.machine;
        }
        
        std::ifstream osFile("/etc/os-release");
        std::string line;
        while (std::getline(osFile, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                static_.osName = line.substr(12);
                if (!static_.osName.empty() && static_.osName.front() == '"') static_.osName.erase(0, 1);
                if (!static_.osName.empty() && static_.osName.back() == '"') static_.osName.pop_back();
            }
        }
    }
    
    std::string name() const override { return "Identity"; }
    
    static std::string formatUptime(int seconds) {
        int days = seconds / 86400;
        int hours = (seconds % 86400) / 3600;
        int mins = (seconds % 3600) / 60;
        
        std::ostringstream oss;
        if (days > 0) oss << days << "d " << hours << "h " << mins << "m";
        else if (hours > 0) oss << hours << "h " << mins << "m";
        else oss << mins << "m";
        return oss.str();
    }
    
protected:
    IdentityData collect() override {
        IdentityData d = static_;
        
        double uptime = 0;
        std::ifstream("/proc/uptime") >> uptime;
        d.uptimeStr = formatUptime(static_cast<int>(uptime));
        
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
        d.currentDate = buf;
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
        d.currentTime = buf;
        
        // Login records: every user session, and those opened from another host (SSH)
        setutent();
        while (struct utmp* entry = getutent()) {
            if (entry->ut_type != USER_PROCESS) continue;
            d.loggedInUsers++;
            if (entry->ut_host[0] != '\0' && entry->ut_host[0] != ':') d.remoteSessions++;
        }
        endutent();
        return d;
    }
    
private:
    IdentityData static_;   // fields that do not change while running
};
