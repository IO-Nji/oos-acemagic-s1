// Unit tests for the parts that parse external input or carry tricky logic.
// No framework: each CHECK reports its own line and the program exits non-zero on failure.
#include "core/config.hpp"
#include "core/dirty_tiles.hpp"
#include "data/health.hpp"
#include "data/providers/docker_provider.hpp"
#include "data/providers/identity_provider.hpp"
#include "data/providers/mqtt_monitor.hpp"
#include "data/providers/network_provider.hpp"
#include "data/providers/services_provider.hpp"

#include <iostream>
#include <vector>

static int failures = 0;
#define CHECK(cond) \
    do { if (!(cond)) { std::cerr << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; failures++; } } while (0)

static void testDirtyTiles() {
    const int W = 320, H = 170;
    std::vector<uint16_t> a(W * H, 0), b(W * H, 0);
    
    CHECK(dirtyTiles(a.data(), b.data(), W, H).empty());
    
    // Unknown panel contents: the whole screen, within the command limits
    auto all = dirtyTiles(nullptr, b.data(), W, H);
    long covered = 0;
    int lastY = -1;
    for (const Tile& t : all) {
        CHECK(t.w * t.h <= tiles::MAX_PIXELS);
        CHECK(t.w <= tiles::MAX_WIDTH && t.w > 0 && t.h > 0);
        CHECK(t.x >= 0 && t.y >= 0 && t.x + t.w <= W && t.y + t.h <= H);
        CHECK(t.y >= lastY);
        lastY = t.y;
        covered += t.w * t.h;
    }
    CHECK(covered == W * H);
    CHECK(all.size() == 30);
    
    // One pixel: one tile of one pixel
    b[50 * W + 100] = 1;
    auto one = dirtyTiles(a.data(), b.data(), W, H);
    CHECK(one.size() == 1);
    CHECK(one[0].x == 100 && one[0].y == 50 && one[0].w == 1 && one[0].h == 1);
    
    // A line of text 8 rows tall starting off the band grid still fits one tile
    for (int y = 55; y < 63; y++) for (int x = 200; x < 300; x++) b[y * W + x] = 2;
    b[50 * W + 100] = 0;
    auto text = dirtyTiles(a.data(), b.data(), W, H);
    CHECK(text.size() == 1);
    CHECK(text[0].x == 200 && text[0].y == 55 && text[0].w == 100 && text[0].h == 8);
    
    // Changes at both edges of one band are split into pieces that fit
    b = a;
    b[10 * W] = 1;
    b[10 * W + W - 1] = 1;
    auto wide = dirtyTiles(a.data(), b.data(), W, H);
    CHECK(wide.size() == 2);
    CHECK(wide[0].x == 0 && wide[0].w + wide[1].w == W);
}

static void testDocker() {
    CHECK(DockerDataProvider::parseUptime("Up 6 weeks (healthy)") == 6 * 7 * 86400);
    CHECK(DockerDataProvider::parseUptime("Up About an hour") == 3600);
    CHECK(DockerDataProvider::parseUptime("Up 59 minutes") == 59 * 60);
    CHECK(DockerDataProvider::parseUptime("Up Less than a second") == 0);
    CHECK(DockerDataProvider::parseUptime("Exited (0) 5 hours ago") == 0);
    
    CHECK(DockerDataProvider::parseMemoryMB("212.4MiB / 15.4GiB") == 212);
    CHECK(DockerDataProvider::parseMemoryMB("1.5GiB / 15.4GiB") == 1536);
    CHECK(DockerDataProvider::parseMemoryMB("900KiB / 1GiB") == 1);
    CHECK(DockerDataProvider::parseMemoryMB("garbage") == 0);
    
    DockerData d = DockerDataProvider::parse(
        "aaaaaaaaaaaa1111|web|nginx:1|Up 2 days|running\n"
        "bbbbbbbbbbbb2222|db|registry.local:5000/team/db:2|Up 2 days (unhealthy)|running\n"
        "cccccccccccc3333|job|busybox|Exited (0) 1 hour ago|exited\n"
        "dddddddddddd4444|busy|app|Up 3 hours|running\n"
        "malformed line\n",
        "aaaaaaaaaaaa|0.50%|10MiB / 1GiB\ndddddddddddd|42.00%|2GiB / 4GiB\n");
    CHECK(d.available);
    CHECK(d.total == 4 && d.running == 3 && d.stopped == 1 && d.paused == 0 && d.unhealthy == 1);
    // Trouble first, then running containers by CPU, then the rest
    CHECK(d.containers[0].name == "db" && d.containers[0].unhealthy);
    CHECK(d.containers[1].name == "busy" && d.containers[1].memUsageMB == 2048);
    CHECK(d.containers[2].name == "web" && d.containers[2].cpuPercent == 0.5f);
    CHECK(d.containers[3].name == "job");
    
    CHECK(DockerDataProvider::parse("", "").total == 0);
}

static void testServices() {
    ServicesData s = ServicesDataProvider::parseShow(
        "Id=ssh.service\nLoadState=loaded\nActiveState=active\n\n"
        "Id=ssh.service\nLoadState=loaded\nActiveState=active\n\n"      // alias of the same unit
        "Id=mysql.service\nLoadState=not-found\nActiveState=inactive\n\n"
        "Id=nginx.service\nLoadState=loaded\nActiveState=failed\n\n"
        "Id=backup.timer\nLoadState=loaded\nActiveState=inactive\n");
    CHECK(s.services.size() == 3);
    CHECK(s.activeCount == 1 && s.failedCount == 1);
    CHECK(s.services[0].name == "ssh");
    CHECK(s.services[2].name == "backup.timer");
    
    CHECK(ServicesDataProvider::isSafeUnitName("postgresql@16-main.service"));
    CHECK(!ServicesDataProvider::isSafeUnitName("ssh; reboot"));
    CHECK(!ServicesDataProvider::isSafeUnitName("--now"));
    CHECK(!ServicesDataProvider::isSafeUnitName(""));
}

static void testConfig() {
    Config c;
    std::vector<std::string> w;
    c.apply("# comment\n"
            "page_interval = 8   # trailing comment\n"
            "pages = identity, system, system\n"
            "services = ssh , cron,\n"
            "disk = /srv\n", w);
    CHECK(w.empty());
    CHECK(c.pageIntervalSec == 8);
    CHECK(c.pages == (std::vector<std::string>{"identity", "system"}));
    CHECK(c.services == (std::vector<std::string>{"ssh", "cron"}));
    CHECK(c.diskPath == "/srv");
    
    // '#' starts a comment only at the start of a line or alone between spaces
    Config m;
    m.apply("mqtt_topics = #\n", w);
    CHECK(m.mqttTopics == (std::vector<std::string>{"#"}));
    m.mqttTopics.clear();
    m.apply("mqtt_topics = #        # everything\n", w);
    CHECK(m.mqttTopics == (std::vector<std::string>{"#"}));
    m.apply("mqtt_topics = a/# #\n", w);
    CHECK(m.mqttTopics == (std::vector<std::string>{"a/#"}));
    m.apply("mqtt_topics = home/#, sensors/+/state   # the ones that matter\nmqtt_port = 8883\npages = mqtt\n", w);
    CHECK(m.mqttTopics == (std::vector<std::string>{"home/#", "sensors/+/state"}));
    CHECK(m.mqttPort == 8883 && m.pages == (std::vector<std::string>{"mqtt"}));
    CHECK(w.empty());
    m.apply("mqtt_port = 99999\n", w);
    CHECK(w.size() == 1 && m.mqttPort == 8883);
    w.clear();
    
    // Bad values are reported and leave the setting alone
    c.apply("page_interval = fast\npages = nope\ndisk = relative\ncolour = red\nno equals sign\n", w);
    CHECK(w.size() == 6);   // "pages" reports the unknown name and the empty result
    CHECK(c.pageIntervalSec == 8);
    CHECK(c.pages.size() == 2);
    CHECK(c.diskPath == "/srv");
    
    Config defaults = Config::defaultsFor("my-unit");
    CHECK(defaults.pages.size() == 6 && defaults.pageIntervalSec == 5);
    CHECK(defaults.services.back() == "my-unit");
    
    // Device settings are collected only when the device names them
    Config device;
    device.deviceKeys = {"panel"};
    w.clear();
    device.apply("panel = /dev/fb1\nother = 1\n", w);
    CHECK(device.deviceSettings["panel"] == "/dev/fb1");
    CHECK(w.size() == 1);
}

static void testHealth() {
    SystemData sys;
    SensorsData sensors;
    DockerData docker;
    ServicesData services;
    
    Health h = health::evaluate(sys, sensors, docker, services);
    CHECK(h.level == Health::Level::Ok && h.summary() == "OK");
    
    sys.disk.usagePercent = 93.4f;
    docker.unhealthy = 2;
    h = health::evaluate(sys, sensors, docker, services);
    CHECK(h.level == Health::Level::Warning);
    CHECK(h.summary() == "DISK 93% +1");
    
    services.failedCount = 1;
    sensors.cpuTempC = 96;
    h = health::evaluate(sys, sensors, docker, services);
    CHECK(h.level == Health::Level::Critical);
    CHECK(h.alerts.size() == 4 && h.alerts[0] == "1 SVC FAILED" && h.alerts[1] == "CPU 96C");
    
    sensors.underVoltage = true;
    h = health::evaluate(sys, sensors, docker, services);
    CHECK(h.alerts.size() == 5);
    
    h = health::evaluate(SystemData(), SensorsData(), DockerData(), ServicesData(), true);
    CHECK(h.level == Health::Level::Critical && h.summary() == "MQTT OFFLINE");
}

static void testMqtt() {
    // CONNECT: fixed header, protocol name and level, clean session, keep-alive, client id
    auto connect = mqtt::connectPacket("abc", "", "", 60);
    const std::vector<uint8_t> expected = {0x10, 15, 0, 4, 'M', 'Q', 'T', 'T', 4, 0x02, 0, 60, 0, 3, 'a', 'b', 'c'};
    CHECK(connect == expected);
    auto withLogin = mqtt::connectPacket("abc", "user", "pw", 60);
    CHECK(withLogin[9] == 0xC2 && withLogin.size() == expected.size() + 6 + 4);
    
    auto subscribe = mqtt::subscribePacket(1, {"#", "$SYS/broker/#"});
    CHECK(subscribe[0] == 0x82 && subscribe[1] == subscribe.size() - 2);
    CHECK(subscribe[2] == 0 && subscribe[3] == 1 && subscribe[6] == '#' && subscribe[7] == 0);
    
    // A stream with a CONNACK, a retained PUBLISH, a QoS 1 PUBLISH and one longer
    // than 127 bytes, delivered one byte at a time
    std::vector<uint8_t> stream = {0x20, 2, 0, 0};
    auto publish = [&](uint8_t flags, const std::string& topic, const std::string& payload) {
        std::vector<uint8_t> body = {static_cast<uint8_t>(topic.size() >> 8), static_cast<uint8_t>(topic.size())};
        body.insert(body.end(), topic.begin(), topic.end());
        if (flags & 0x06) { body.push_back(0); body.push_back(7); }
        body.insert(body.end(), payload.begin(), payload.end());
        auto packet = mqtt::detail::frame(0x30 | flags, body);
        stream.insert(stream.end(), packet.begin(), packet.end());
    };
    publish(0x01, "a/b", "on");
    publish(0x02, "c", "qos1");
    publish(0x00, "big", std::string(1000, 'x'));
    
    mqtt::Parser parser;
    std::vector<mqtt::Packet> got;
    for (uint8_t b : stream) parser.feed(&b, 1, [&](const mqtt::Packet& p) { got.push_back(p); });
    CHECK(got.size() == 4);
    CHECK(got[0].type == mqtt::CONNACK && got[0].returnCode == 0);
    CHECK(got[1].topic == "a/b" && got[1].payload == "on" && got[1].retained);
    CHECK(got[2].topic == "c" && got[2].payload == "qos1" && !got[2].retained);
    CHECK(got[3].topic == "big" && got[3].payload.size() == mqtt::Parser::MAX_PAYLOAD);
    
    // The monitor sorts broker statistics from ordinary messages
    MqttMonitor monitor{Config()};
    auto deliver = [&](const std::string& topic, const std::string& payload) {
        mqtt::Packet p;
        p.type = mqtt::PUBLISH;
        p.topic = topic;
        p.payload = payload;
        monitor.handlePublish(p);
    };
    deliver("$SYS/broker/version", "mosquitto version 2.0.11");
    deliver("$SYS/broker/clients/connected", "3");
    deliver("$SYS/broker/uptime", "7200 seconds");
    deliver("$SYS/broker/load/messages/received/1min", "12.50");
    deliver("room/temp", "21.5\n");
    deliver("room/temp", "21.6");
    deliver("room/raw", std::string("a\x01\tb"));
    MqttData d = monitor.data();
    CHECK(d.version == "2.0.11" && d.clientsConnected == 3 && d.uptimeSeconds == 7200);
    CHECK(d.receivedPerMin == 12.5f && d.clientsTotal == -1);
    CHECK(d.recent.size() == 3 && d.messageCount == 3 && d.topicCount == 2);
    CHECK(d.recent[0].payload == "21.5" && d.recent[2].payload == "a? b");
    
    for (int i = 0; i < 100; i++) deliver("flood", std::to_string(i));
    d = monitor.data();
    CHECK(d.recent.size() == MqttMonitor::RECENT_MESSAGES && d.recent.back().payload == "99");
}

static void testMisc() {
    CHECK(NetworkDataProvider::isVirtualInterface("docker0"));
    CHECK(NetworkDataProvider::isVirtualInterface("br-24eddd6dfe72"));
    CHECK(!NetworkDataProvider::isVirtualInterface("enp2s0"));
    CHECK(!NetworkDataProvider::isVirtualInterface("wg0"));
    
    CHECK(IdentityDataProvider::formatUptime(70 * 86400 + 9 * 3600 + 120) == "70d 9h 2m");
    CHECK(IdentityDataProvider::formatUptime(3700) == "1h 1m");
    CHECK(IdentityDataProvider::formatUptime(59) == "0m");
}

int main() {
    testDirtyTiles();
    testDocker();
    testServices();
    testConfig();
    testHealth();
    testMqtt();
    testMisc();
    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all tests passed\n";
    return 0;
}
