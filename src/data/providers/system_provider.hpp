#pragma once
#include "data/provider_base.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/statvfs.h>
#include <unistd.h>

// CPU, memory, one filesystem and uptime from /proc and /sys
class SystemDataProvider : public SnapshotProvider<SystemData> {
public:
    explicit SystemDataProvider(std::string diskPath = "/") : diskPath_(std::move(diskPath)) {
        // Prime the CPU counters so the first reading is a real delta
        readCpuUsage();
        char hostname[256];
        hostname_ = gethostname(hostname, sizeof(hostname)) == 0 ? hostname : "unknown";
    }
    
    std::string name() const override { return "System"; }
    
protected:
    SystemData collect() override {
        SystemData d;
        d.cpu.usagePercent = readCpuUsage();
        d.cpu.freqMHz = readCpuFreq();
        
        std::ifstream loadavg("/proc/loadavg");
        loadavg >> d.cpu.loadAvg1;
        
        readMemory(d.memory);
        readDisk(d.disk);
        
        d.hostname = hostname_;
        double uptime = 0;
        std::ifstream("/proc/uptime") >> uptime;
        d.uptimeSeconds = static_cast<int>(uptime);
        return d;
    }
    
private:
    std::string diskPath_;
    std::string hostname_;
    uint64_t prevCpuTotal_ = 0;
    uint64_t prevCpuIdle_ = 0;
    
    float readCpuUsage() {
        std::ifstream file("/proc/stat");
        std::string cpu;
        uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
        if (!(file >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
            return 0.0f;
        }
        
        uint64_t idleTime = idle + iowait;
        uint64_t totalTime = user + nice + system + idleTime + irq + softirq + steal;
        uint64_t totalDelta = totalTime - prevCpuTotal_;
        uint64_t idleDelta = idleTime - prevCpuIdle_;
        prevCpuTotal_ = totalTime;
        prevCpuIdle_ = idleTime;
        
        if (totalDelta == 0) return 0.0f;
        return 100.0f * (1.0f - static_cast<float>(idleDelta) / static_cast<float>(totalDelta));
    }
    
    // Mean of the current frequency of every core
    int readCpuFreq() {
        long sumKHz = 0;
        int cores = 0;
        for (int i = 0; i < 256; i++) {
            std::ifstream file("/sys/devices/system/cpu/cpu" + std::to_string(i) +
                               "/cpufreq/scaling_cur_freq");
            long kHz = 0;
            if (!(file >> kHz)) break;
            sumKHz += kHz;
            cores++;
        }
        return cores ? static_cast<int>(sumKHz / cores / 1000) : 0;
    }
    
    void readMemory(MemoryData& mem) {
        std::ifstream file("/proc/meminfo");
        std::string line;
        long total = 0, available = 0, buffers = 0, cached = 0, swapTotal = 0, swapFree = 0;
        
        while (std::getline(file, line)) {
            if (sscanf(line.c_str(), "MemTotal: %ld kB", &total) == 1) continue;
            if (sscanf(line.c_str(), "MemAvailable: %ld kB", &available) == 1) continue;
            if (sscanf(line.c_str(), "Buffers: %ld kB", &buffers) == 1) continue;
            if (sscanf(line.c_str(), "Cached: %ld kB", &cached) == 1) continue;
            if (sscanf(line.c_str(), "SwapTotal: %ld kB", &swapTotal) == 1) continue;
            sscanf(line.c_str(), "SwapFree: %ld kB", &swapFree);
        }
        
        mem.totalMB = static_cast<int>(total / 1024);
        mem.usedMB = static_cast<int>((total - available) / 1024);
        mem.buffersMB = static_cast<int>(buffers / 1024);
        mem.cachedMB = static_cast<int>(cached / 1024);
        mem.usagePercent = total > 0 ? 100.0f * (total - available) / total : 0.0f;
        mem.swapUsagePercent = swapTotal > 0 ? 100.0f * (swapTotal - swapFree) / swapTotal : 0.0f;
    }
    
    void readDisk(DiskData& disk) {
        disk.mountPoint = diskPath_;
        struct statvfs stat;
        if (statvfs(diskPath_.c_str(), &stat) != 0) return;
        
        const uint64_t gib = 1024ull * 1024 * 1024;
        uint64_t total = static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
        uint64_t used = total - static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
        uint64_t available = static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
        
        disk.totalGB = static_cast<int>(total / gib);
        disk.usedGB = static_cast<int>(used / gib);
        // Same figure as df: blocks reserved for root count as unavailable
        disk.usagePercent = used + available > 0 ? 100.0f * used / (used + available) : 0.0f;
    }
};
