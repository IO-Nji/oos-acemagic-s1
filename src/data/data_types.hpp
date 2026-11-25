#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Plain data produced by the providers and drawn by the screens.

class DataProvider {
public:
    virtual ~DataProvider() = default;
    virtual void update() = 0;
    virtual std::string name() const = 0;
};

struct CpuData {
    float usagePercent = 0.0f;
    int freqMHz = 0;           // average over all cores
    float loadAvg1 = 0.0f;
};

struct MemoryData {
    float usagePercent = 0.0f;
    int usedMB = 0;
    int totalMB = 0;
    int cachedMB = 0;
    int buffersMB = 0;
    float swapUsagePercent = 0.0f;
};

struct DiskData {
    std::string mountPoint;
    float usagePercent = 0.0f;
    int usedGB = 0;
    int totalGB = 0;
};

struct SystemData {
    CpuData cpu;
    MemoryData memory;
    DiskData disk;
    std::string hostname;
    int uptimeSeconds = 0;
};

struct NetworkInterfaceData {
    std::string name;
    std::string ipAddress;
    uint64_t rxBps = 0;
    uint64_t txBps = 0;
    uint64_t rxBytes = 0;
    uint64_t txBytes = 0;
    bool isUp = false;
    bool isVirtual = false;    // container bridge or similar; not counted in totals
};

struct NetworkData {
    std::vector<NetworkInterfaceData> interfaces;   // physical first
    std::string primaryIp;
    uint64_t totalRxBps = 0;
    uint64_t totalTxBps = 0;
    uint64_t totalRxBytes = 0;
    uint64_t totalTxBytes = 0;
};

struct DockerContainerData {
    std::string id;
    std::string name;
    std::string image;
    std::string status;        // running, exited, paused, restarting, dead, created
    bool unhealthy = false;    // health check failing
    int uptimeSeconds = 0;
    float cpuPercent = 0.0f;
    int memUsageMB = 0;
};

struct DockerData {
    bool available = false;    // docker CLI answered
    int total = 0;
    int running = 0;
    int stopped = 0;
    int paused = 0;
    int unhealthy = 0;         // unhealthy, restarting or dead
    std::vector<DockerContainerData> containers;    // problems first, then by CPU use
};

struct ServiceData {
    std::string name;
    std::string status;        // active, inactive, failed, activating, ...
};

struct ServicesData {
    std::vector<ServiceData> services;
    int activeCount = 0;
    int failedCount = 0;
};

struct SensorReading {
    std::string name;
    float value = 0.0f;
};

struct SensorsData {
    std::vector<SensorReading> temperatures;
    std::vector<SensorReading> fans;
    std::vector<SensorReading> power;
    float cpuTempC = 0.0f;     // package temperature; 0 when unknown
    float gpuTempC = 0.0f;
    float ssdTempC = 0.0f;
    float totalPowerW = 0.0f;  // CPU package power
    float tdpW = 0.0f;         // CPU rated power
};

struct IdentityData {
    std::string hostname;
    std::string osName;
    std::string kernelVersion;
    std::string architecture;
    std::string uptimeStr;
    std::string currentTime;
    std::string currentDate;
    int loggedInUsers = 0;
    int remoteSessions = 0;    // logins that came from another host
};
