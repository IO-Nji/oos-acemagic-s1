#pragma once
// Fixed data sets for repeatable preview renders.
//   typical: an S1 running a handful of containers
//   stress:  worst-case widths (100 %, long names, large units) and every alert,
//            for the layout checks
#include "data/data_types.hpp"
#include "data/providers/docker_provider.hpp"
#include "data/providers/services_provider.hpp"
#include <cmath>

namespace mock {

struct Set {
    SystemData system;
    NetworkData network;
    DockerData docker;
    ServicesData services;
    IdentityData identity;
    SensorsData sensors;
    MqttData mqtt;
};

inline NetworkInterfaceData iface(const char* name, const char* ip, bool up,
                                  uint64_t rxBps, uint64_t txBps, uint64_t rx, uint64_t tx) {
    NetworkInterfaceData i;
    i.name = name; i.ipAddress = ip; i.isUp = up;
    i.rxBps = rxBps; i.txBps = txBps; i.rxBytes = rx; i.txBytes = tx;
    return i;
}

// Recompute the totals that depend on the interface list
inline void finish(Set& s) {
    s.network.totalRxBps = s.network.totalTxBps = 0;
    s.network.totalRxBytes = s.network.totalTxBytes = 0;
    for (const auto& i : s.network.interfaces) {
        s.network.totalRxBps += i.rxBps;
        s.network.totalTxBps += i.txBps;
        s.network.totalRxBytes += i.rxBytes;
        s.network.totalTxBytes += i.txBytes;
    }
}

inline Set typical() {
    Set s;
    s.system.cpu.usagePercent = 37.4f;
    s.system.cpu.freqMHz = 2890;
    s.system.cpu.loadAvg1 = 1.24f;
    s.system.memory.usagePercent = 37.9f;
    s.system.memory.usedMB = 5968;
    s.system.memory.totalMB = 15766;
    s.system.memory.cachedMB = 6898;
    s.system.memory.buffersMB = 310;
    s.system.memory.swapUsagePercent = 12.0f;
    s.system.disk = {"/", 51.0f, 224, 468};
    s.system.hostname = "s1-mini";
    s.system.uptimeSeconds = 18 * 86400 + 9 * 3600 + 2 * 60;

    s.network.primaryIp = "192.168.1.20";
    s.network.interfaces = {
        iface("enp2s0", "192.168.1.20", true, 183400, 42100, 81234567890ull, 12345678901ull),
        iface("wlp1s0", "192.168.1.21", true, 2048, 912, 3456789012ull, 987654321ull),
        iface("wg0", "10.0.0.1", true, 0, 0, 123456789ull, 23456789ull),
    };
    s.network.interfaces[1].signalDbm = -58;

    s.docker = DockerDataProvider::parse(
        "a1|nextcloud|nextcloud:29-apache|Up 2 weeks (healthy)|running\n"
        "a2|nextcloud-db|mariadb:11|Up 2 weeks (healthy)|running\n"
        "a3|jellyfin|jellyfin/jellyfin:latest|Up 2 weeks (healthy)|running\n"
        "a4|traefik|traefik:v3|Up 2 weeks|running\n"
        "a5|grafana|grafana/grafana:11.2.0|Up 6 days|running\n"
        "a6|prometheus|prom/prometheus:v2.54.1|Up 6 days|running\n"
        "a7|backup|restic/restic:0.17|Exited (0) 5 hours ago|exited\n",
        "a1|1.8%|388MiB / 15.4GiB\na2|0.4%|212MiB / 15.4GiB\na3|6.3%|1.1GiB / 15.4GiB\n"
        "a4|0.1%|54MiB / 15.4GiB\na5|0.3%|96MiB / 15.4GiB\na6|0.9%|240MiB / 15.4GiB\n");

    s.services.services = {{"ssh", "active"}, {"docker", "active"},
                           {"oos-acemagic-s1", "active"}, {"nginx", "inactive"}};
    s.services.activeCount = 3;

    s.identity.hostname = "s1-mini";
    s.identity.osName = "Ubuntu 24.04.1 LTS";
    s.identity.kernelVersion = "6.8.0-45-generic";
    s.identity.architecture = "x86_64";
    s.identity.uptimeStr = "18d 9h 2m";
    s.identity.currentTime = "12:34:56";
    s.identity.currentDate = "2025-01-15";
    s.identity.loggedInUsers = 2;
    s.identity.remoteSessions = 1;

    s.sensors.temperatures = {
        {"Composite", 43.9f}, {"Sensor 1", 51.9f}, {"Package id 0", 49.0f},
        {"Core 0", 48.0f}, {"Core 1", 47.0f}, {"Core 2", 48.0f}, {"Core 3", 46.0f},
    };
    s.sensors.power = {{"package-0", 7.8f}, {"core", 3.1f}, {"uncore", 0.2f}};
    s.sensors.cpuTempC = 49.0f;
    s.sensors.ssdTempC = 43.9f;
    s.sensors.totalPowerW = 7.8f;
    s.sensors.tdpW = 12.0f;

    s.mqtt.state = MqttData::State::Online;
    s.mqtt.broker = "127.0.0.1:1883";
    s.mqtt.version = "2.0.18";
    s.mqtt.clientsConnected = 5;
    s.mqtt.clientsTotal = 7;
    s.mqtt.subscriptions = 12;
    s.mqtt.receivedPerMin = 42.6f;
    s.mqtt.sentPerMin = 118.0f;
    s.mqtt.uptimeSeconds = 18 * 86400 + 9 * 3600;
    s.mqtt.recent = {
        {"home/kitchen/temperature", "21.4", false},
        {"home/kitchen/humidity", "48", false},
        {"home/door/front", "closed", true},
        {"sensors/garage/state", "{\"motion\":false,\"lux\":12}", false},
        {"home/livingroom/temperature", "22.1", false},
        {"plugs/desk/power", "63.5", false},
        {"home/kitchen/temperature", "21.5", false},
        {"sensors/garden/soil", "{\"moisture\":37,\"battery\":91}", false},
    };
    s.mqtt.messageCount = 15234;
    s.mqtt.topicCount = 23;

    finish(s);
    return s;
}

inline Set stress() {
    Set s;
    s.system.cpu.usagePercent = 100.0f;
    s.system.cpu.freqMHz = 4800;
    s.system.cpu.loadAvg1 = 88.88f;
    s.system.memory.usagePercent = 100.0f;
    s.system.memory.usedMB = 130048;
    s.system.memory.totalMB = 130048;
    s.system.memory.cachedMB = 88888;
    s.system.memory.buffersMB = 2048;
    s.system.memory.swapUsagePercent = 100.0f;
    s.system.disk = {"/", 100.0f, 8888, 8888};
    s.system.hostname = "very-long-hostname-01";
    s.system.uptimeSeconds = 888 * 86400 + 23 * 3600 + 59 * 60;

    s.network.primaryIp = "255.255.255.255";
    s.network.interfaces = {
        iface("enx00e04c680001", "255.255.255.255", true, 999ull << 20, 999ull << 20, 888ull << 40, 888ull << 40),
        iface("wlp0s20f3", "10.88.88.88", false, 999ull << 10, 999ull << 10, 888ull << 30, 888ull << 30),
        iface("wg0", "10.88.88.1", true, 888, 888, 888, 888),
        iface("docker0", "172.17.0.1", true, 0, 0, 0, 0),
    };

    s.docker = DockerDataProvider::parse(
        "b1|order-processing-service-blue|registry.example.com/shop/order-processing-service:latest|Up 888 days (unhealthy)|running\n"
        "b2|inventory-service|registry.example.com/very-long-image-name:latest|Up 2 hours (Paused)|paused\n"
        "b3|exited-container-name|mariadb:11|Exited (137) 2 days ago|exited\n"
        "b4|restarting-container|redis:7-alpine|Restarting (1) 5 seconds ago|restarting\n"
        "b5|fifth|traefik:v3|Up 59 minutes|running\n"
        "b6|sixth|traefik:v3|Dead|dead\n",
        "b1|100.00%|88888MiB / 127GiB\nb5|88.80%|888MiB / 127GiB\n");

    s.services = ServicesDataProvider::parseShow(
        "Id=systemd-networkd-wait-online.service\nLoadState=loaded\nActiveState=active\n\n"
        "Id=docker.service\nLoadState=loaded\nActiveState=failed\n\n"
        "Id=nginx.service\nLoadState=loaded\nActiveState=inactive\n\n"
        "Id=postgresql@16-main.service\nLoadState=loaded\nActiveState=activating\n\n"
        "Id=unattended-upgrades.service\nLoadState=loaded\nActiveState=deactivating\n\n"
        "Id=oos-acemagic-s1.service\nLoadState=loaded\nActiveState=active\n\n"
        "Id=snap.adguard-home.adguard-home.service\nLoadState=loaded\nActiveState=failed\n\n"
        "Id=ssh.service\nLoadState=loaded\nActiveState=reloading\n\n"
        "Id=glances.service\nLoadState=loaded\nActiveState=active\n\n"
        "Id=jellyfin.service\nLoadState=loaded\nActiveState=active\n\n"
        "Id=cloudflared.service\nLoadState=loaded\nActiveState=unknown\n\n"
        "Id=fail2ban.service\nLoadState=loaded\nActiveState=active\n\n"
        "Id=cron.service\nLoadState=loaded\nActiveState=active\n\n"
        "Id=thirteenth-over-capacity.service\nLoadState=loaded\nActiveState=active\n");

    s.identity.hostname = "very-long-hostname-01";
    s.identity.osName = "Ubuntu 24.04.1 LTS (Noble Numbat) Extended";
    s.identity.kernelVersion = "6.8.0-45-generic-lowlatency-hwe";
    s.identity.architecture = "x86_64";
    s.identity.uptimeStr = "888d 23h 59m";
    s.identity.currentTime = "23:59:59";
    s.identity.currentDate = "2025-12-28";
    s.identity.loggedInUsers = 88;
    s.identity.remoteSessions = 88;

    s.sensors.temperatures = {
        {"Composite", 100.0f}, {"Sensor 1", 88.0f}, {"Package id 0", 100.0f}, {"Core 0", 100.0f},
        {"iwlwifi_1 1", 88.0f}, {"acpitz 1", 68.0f}, {"GPU edge", 100.0f},
    };
    s.sensors.fans = {{"cpu_fan", 8888.0f}, {"thinkpad 1", 0.0f}, {"sys", 1200.0f}, {"aux", 888.0f}};
    s.sensors.power = {{"package-0", 188.8f}, {"core", 88.8f}, {"uncore", 8.8f}, {"dram-long-name", 18.8f}};
    s.sensors.cpuTempC = 100.0f;
    s.sensors.gpuTempC = 100.0f;
    s.sensors.ssdTempC = 100.0f;
    s.sensors.totalPowerW = 188.8f;
    s.sensors.tdpW = 12.0f;
    s.sensors.underVoltage = true;

    s.mqtt.state = MqttData::State::Online;
    s.mqtt.broker = "broker.example.com:8883";
    s.mqtt.version = "2.0.18";
    s.mqtt.clientsConnected = 8888;
    s.mqtt.clientsTotal = 88888;
    s.mqtt.receivedPerMin = 88888.0f;
    s.mqtt.sentPerMin = 88888.0f;
    s.mqtt.uptimeSeconds = 888L * 86400 + 23 * 3600;
    for (int i = 0; i < 12; i++) {
        s.mqtt.recent.push_back({"building/floor-" + std::to_string(i) + "/room/sensor/temperature/celsius",
                                 "{\"value\":88.88,\"unit\":\"C\",\"battery\":100,\"rssi\":-88}", i % 3 == 0});
    }
    s.mqtt.recent.push_back({"short", "", false});
    s.mqtt.messageCount = 888888888;
    s.mqtt.topicCount = 2000;
    s.network.interfaces[1].signalDbm = -100;

    finish(s);
    return s;
}

// Deterministic waveform in [0,1] for history graphs
inline float wave(int step, float phase = 0.0f) {
    float t = step * 0.37f + phase;
    return 0.5f + 0.32f * std::sin(t) + 0.18f * std::sin(t * 2.7f + 1.0f);
}

}  // namespace mock
