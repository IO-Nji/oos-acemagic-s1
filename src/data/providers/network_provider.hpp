#pragma once
#include "data/provider_base.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

// IPv4 interfaces with throughput derived from the kernel byte counters
class NetworkDataProvider : public SnapshotProvider<NetworkData> {
public:
    NetworkDataProvider() {
        // Prime the counters so the first reading is a real delta
        collect();
    }
    
    std::string name() const override { return "Network"; }
    
    // Container bridges and virtual ethernet pairs carry traffic that is already
    // counted on a physical interface
    static bool isVirtualInterface(const std::string& name) {
        for (const char* prefix : {"docker", "br-", "veth", "virbr", "cni", "flannel", "podman"}) {
            if (name.compare(0, std::char_traits<char>::length(prefix), prefix) == 0) return true;
        }
        return false;
    }
    
protected:
    NetworkData collect() override {
        NetworkData d;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - prevTime_).count();
        prevTime_ = now;
        
        struct ifaddrs* ifaddr;
        if (getifaddrs(&ifaddr) != 0) return d;
        
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            
            NetworkInterfaceData iface;
            iface.name = ifa->ifa_name;
            iface.isUp = (ifa->ifa_flags & IFF_UP) != 0;
            iface.isVirtual = isVirtualInterface(iface.name);
            
            char addr[INET_ADDRSTRLEN] = "";
            inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr,
                      addr, sizeof(addr));
            iface.ipAddress = addr;
            
            iface.rxBytes = readCounter(iface.name, "rx_bytes");
            iface.txBytes = readCounter(iface.name, "tx_bytes");
            
            // Rate since the previous sample; skipped when the counters restarted
            auto it = prevBytes_.find(iface.name);
            if (it != prevBytes_.end() && elapsed > 0 &&
                iface.rxBytes >= it->second.first && iface.txBytes >= it->second.second) {
                iface.rxBps = (iface.rxBytes - it->second.first) * 1000 / elapsed;
                iface.txBps = (iface.txBytes - it->second.second) * 1000 / elapsed;
            }
            prevBytes_[iface.name] = {iface.rxBytes, iface.txBytes};
            
            if (!iface.isVirtual) {
                d.totalRxBps += iface.rxBps;
                d.totalTxBps += iface.txBps;
                d.totalRxBytes += iface.rxBytes;
                d.totalTxBytes += iface.txBytes;
                if (d.primaryIp.empty() && iface.isUp) d.primaryIp = iface.ipAddress;
            }
            
            d.interfaces.push_back(iface);
        }
        freeifaddrs(ifaddr);
        
        std::stable_sort(d.interfaces.begin(), d.interfaces.end(),
                         [](const NetworkInterfaceData& a, const NetworkInterfaceData& b) {
                             return !a.isVirtual && b.isVirtual;
                         });
        return d;
    }
    
private:
    std::chrono::steady_clock::time_point prevTime_ = std::chrono::steady_clock::now();
    std::map<std::string, std::pair<uint64_t, uint64_t>> prevBytes_;
    
    static uint64_t readCounter(const std::string& iface, const char* counter) {
        uint64_t value = 0;
        std::ifstream("/sys/class/net/" + iface + "/statistics/" + counter) >> value;
        return value;
    }
};
