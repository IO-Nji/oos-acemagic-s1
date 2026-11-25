#pragma once
#include "data/provider_base.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// Temperatures and fans from hwmon, CPU power from Intel RAPL. RAPL counters are
// readable by root only; without them power is reported as unavailable.
class SensorsDataProvider : public SnapshotProvider<SensorsData> {
public:
    SensorsDataProvider() {
        discoverHwmon();
        discoverRapl();
    }
    
    std::string name() const override { return "Sensors"; }
    
protected:
    SensorsData collect() override {
        SensorsData d;
        readTemperatures(d);
        
        for (const auto& fan : fans_) {
            long rpm = 0;
            if (readNumber(fan.inputPath, rpm)) d.fans.push_back({fan.name, static_cast<float>(rpm)});
        }
        
        readPower(d);
        return d;
    }
    
private:
    struct Sensor {
        std::string inputPath;
        std::string name;
        std::string device;    // hwmon driver name, e.g. coretemp, nvme, amdgpu
    };
    struct RaplDomain {
        std::string path;
        std::string name;
        uint64_t maxEnergyUj = 0;
        float limitW = 0;
        uint64_t lastEnergyUj = 0;
        bool primed = false;
    };
    
    std::vector<Sensor> temps_, fans_;
    std::vector<RaplDomain> rapl_;
    std::chrono::steady_clock::time_point lastRaplTime_;
    
    static std::string readLine(const std::string& path) {
        std::ifstream file(path);
        std::string line;
        std::getline(file, line);
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
        return line;
    }
    
    static bool readNumber(const std::string& path, long& value) {
        std::ifstream file(path);
        return static_cast<bool>(file >> value);
    }
    
    static bool readNumber(const std::string& path, uint64_t& value) {
        std::ifstream file(path);
        return static_cast<bool>(file >> value);
    }
    
    void discoverHwmon() {
        namespace fs = std::filesystem;
        std::error_code ec;
        std::vector<std::string> devices;
        for (const auto& entry : fs::directory_iterator("/sys/class/hwmon", ec)) {
            devices.push_back(entry.path().string());
        }
        std::sort(devices.begin(), devices.end());
        
        for (const auto& dir : devices) {
            std::string device = readLine(dir + "/name");
            for (const char* kind : {"temp", "fan"}) {
                for (int i = 1; i <= 32; i++) {
                    std::string base = dir + "/" + kind + std::to_string(i);
                    if (!fs::exists(base + "_input", ec)) continue;
                    std::string label = readLine(base + "_label");
                    if (label.empty()) label = device + " " + std::to_string(i);
                    Sensor s{base + "_input", label, device};
                    (kind[0] == 't' ? temps_ : fans_).push_back(s);
                }
            }
        }
    }
    
    void discoverRapl() {
        namespace fs = std::filesystem;
        std::error_code ec;
        const std::string base = "/sys/class/powercap";
        for (const auto& entry : fs::directory_iterator(base, ec)) {
            std::string dir = entry.path().string();
            if (entry.path().filename().string().rfind("intel-rapl:", 0) != 0) continue;
            
            RaplDomain domain;
            domain.path = dir;
            domain.name = readLine(dir + "/name");
            readNumber(dir + "/max_energy_range_uj", domain.maxEnergyUj);
            long limitUw = 0;
            if (readNumber(dir + "/constraint_0_max_power_uw", limitUw)) domain.limitW = limitUw / 1e6f;
            if (!domain.name.empty()) rapl_.push_back(domain);
        }
        std::sort(rapl_.begin(), rapl_.end(),
                  [](const RaplDomain& a, const RaplDomain& b) { return a.path < b.path; });
    }
    
    static bool contains(const std::string& s, const char* part) {
        return s.find(part) != std::string::npos;
    }
    
    void readTemperatures(SensorsData& d) {
        float coreMax = 0;
        for (const auto& sensor : temps_) {
            long milli = 0;
            if (!readNumber(sensor.inputPath, milli)) continue;
            float temp = milli / 1000.0f;
            d.temperatures.push_back({sensor.name, temp});
            
            bool cpuDevice = sensor.device == "coretemp" || sensor.device == "k10temp" ||
                             sensor.device == "zenpower";
            if (cpuDevice && (contains(sensor.name, "Package") || contains(sensor.name, "Tctl"))) {
                d.cpuTempC = std::max(d.cpuTempC, temp);
            } else if (cpuDevice) {
                coreMax = std::max(coreMax, temp);
            } else if (sensor.device == "nvme" && (d.ssdTempC == 0 || contains(sensor.name, "Composite"))) {
                d.ssdTempC = temp;
            } else if (d.gpuTempC == 0 && (sensor.device == "amdgpu" || sensor.device == "nouveau" ||
                                           sensor.device == "nvidia" || contains(sensor.name, "GPU"))) {
                d.gpuTempC = temp;
            }
        }
        if (d.cpuTempC == 0) d.cpuTempC = coreMax;
    }
    
    // Power is the energy counter's increase divided by the time between two reads
    void readPower(SensorsData& d) {
        auto now = std::chrono::steady_clock::now();
        double seconds = std::chrono::duration<double>(now - lastRaplTime_).count();
        lastRaplTime_ = now;
        
        for (auto& domain : rapl_) {
            uint64_t energy = 0;
            if (!readNumber(domain.path + "/energy_uj", energy)) continue;
            
            if (domain.primed && seconds > 0) {
                uint64_t delta = energy >= domain.lastEnergyUj
                                     ? energy - domain.lastEnergyUj
                                     : energy + domain.maxEnergyUj - domain.lastEnergyUj;   // counter wrapped
                float watts = static_cast<float>(delta / 1e6 / seconds);
                d.power.push_back({domain.name, watts});
                
                // Top-level domains are the CPU packages; their subdomains are parts of them
                if (domain.name.rfind("package", 0) == 0) {
                    d.totalPowerW += watts;
                    d.tdpW += domain.limitW;
                }
            }
            domain.lastEnergyUj = energy;
            domain.primed = true;
        }
    }
};
