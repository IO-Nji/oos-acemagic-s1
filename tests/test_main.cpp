// Unit tests for the parts that parse external input or carry tricky logic.
// No framework: each CHECK reports its own line and the program exits non-zero on failure.
#include "core/config.hpp"
#include "core/dirty_tiles.hpp"
#include "data/health.hpp"
#include "data/providers/docker_provider.hpp"
#include "data/providers/identity_provider.hpp"
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
    
    // Bad values are reported and leave the setting alone
    c.apply("page_interval = fast\npages = nope\ndisk = relative\ncolour = red\nno equals sign\n", w);
    CHECK(w.size() == 6);   // "pages" reports the unknown name and the empty result
    CHECK(c.pageIntervalSec == 8);
    CHECK(c.pages.size() == 2);
    CHECK(c.diskPath == "/srv");
    
    Config defaults;
    CHECK(defaults.pages.size() == 6 && defaults.pageIntervalSec == 5);
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
    testMisc();
    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all tests passed\n";
    return 0;
}
