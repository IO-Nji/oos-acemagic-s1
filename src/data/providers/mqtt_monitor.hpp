#pragma once
#include "core/config.hpp"
#include "data/data_types.hpp"
#include "data/mqtt/mqtt_packets.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// Watches an MQTT broker: keeps a connection open, records the broker's own
// statistics ($SYS topics) and the latest messages on the configured topics.
// Read-only: it never publishes. Reconnects by itself when the broker goes away.
class MqttMonitor {
public:
    static constexpr size_t RECENT_MESSAGES = 40;
    static constexpr size_t MAX_TRACKED_TOPICS = 2000;
    
    explicit MqttMonitor(const Config& config)
        : host_(config.mqttHost), port_(config.mqttPort), user_(config.mqttUser),
          password_(config.mqttPassword), topics_(config.mqttTopics) {
        data_.broker = host_ + ":" + std::to_string(port_);
    }
    
    ~MqttMonitor() { stop(); }
    
    void start() {
        if (thread_.joinable() || frozen_) return;
        running_ = true;
        thread_ = std::thread([this] { run(); });
    }
    
    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }
    
    MqttData data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }
    
    // Preview/test hook: show fixed data and never connect
    void setData(const MqttData& d) {
        std::lock_guard<std::mutex> lock(mutex_);
        frozen_ = true;
        data_ = d;
    }
    
    // Record one received message. Public so it can be exercised without a broker.
    void handlePublish(const mqtt::Packet& p) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (p.topic.compare(0, 5, "$SYS/") == 0) {
            handleStatistic(p.topic, p.payload);
            return;
        }
        
        MqttMessage m;
        m.topic = p.topic;
        m.payload = printable(p.payload);
        m.retained = p.retained;
        data_.recent.push_back(m);
        if (data_.recent.size() > RECENT_MESSAGES) data_.recent.erase(data_.recent.begin());
        
        data_.messageCount++;
        if (seenTopics_.size() < MAX_TRACKED_TOPICS) seenTopics_.insert(p.topic);
        data_.topicCount = static_cast<int>(seenTopics_.size());
    }
    
    // Text safe to draw: one line, printable ASCII
    static std::string printable(const std::string& raw) {
        std::string out;
        bool lastSpace = false;
        for (unsigned char c : raw) {
            bool space = c == ' ' || c == '\n' || c == '\r' || c == '\t';
            if (space) {
                if (!lastSpace && !out.empty()) out += ' ';
            } else {
                out += (c < 32 || c > 126) ? '?' : static_cast<char>(c);
            }
            lastSpace = space;
        }
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out;
    }
    
private:
    static constexpr int KEEP_ALIVE_SEC = 60;
    
    void handleStatistic(const std::string& topic, const std::string& value) {
        const std::string name = topic.substr(std::strlen("$SYS/broker/"));
        if (name == "version") {
            // "mosquitto version 2.0.11" -> "2.0.11"
            size_t space = value.rfind(' ');
            data_.version = space == std::string::npos ? value : value.substr(space + 1);
        } else if (name == "uptime") {
            data_.uptimeSeconds = std::atol(value.c_str());
        } else if (name == "clients/connected") {
            data_.clientsConnected = std::atoi(value.c_str());
        } else if (name == "clients/total") {
            data_.clientsTotal = std::atoi(value.c_str());
        } else if (name == "subscriptions/count") {
            data_.subscriptions = std::atoi(value.c_str());
        } else if (name == "load/messages/received/1min") {
            data_.receivedPerMin = static_cast<float>(std::atof(value.c_str()));
        } else if (name == "load/messages/sent/1min") {
            data_.sentPerMin = static_cast<float>(std::atof(value.c_str()));
        }
    }
    
    void setState(MqttData::State state) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.state = state;
        if (state != MqttData::State::Online) {
            // Figures from a broker we are no longer connected to would mislead
            data_.clientsConnected = data_.clientsTotal = data_.subscriptions = -1;
            data_.receivedPerMin = data_.sentPerMin = -1;
            data_.uptimeSeconds = -1;
        }
    }
    
    int connectSocket() {
        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result) != 0) return -1;
        
        int fd = -1;
        for (struct addrinfo* a = result; a != nullptr && fd < 0; a = a->ai_next) {
            fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
            if (fd < 0) continue;
            struct timeval timeout{5, 0};
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            if (connect(fd, a->ai_addr, a->ai_addrlen) != 0) {
                close(fd);
                fd = -1;
            }
        }
        freeaddrinfo(result);
        return fd;
    }
    
    static bool sendAll(int fd, const std::vector<uint8_t>& bytes) {
        size_t sent = 0;
        while (sent < bytes.size()) {
            ssize_t n = send(fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
            if (n <= 0) return false;
            sent += static_cast<size_t>(n);
        }
        return true;
    }
    
    // One connection, from connect to failure or stop
    void session() {
        int fd = connectSocket();
        if (fd < 0) return;
        
        char hostname[64] = "host";
        gethostname(hostname, sizeof(hostname) - 1);
        std::string clientId = std::string("oos-") + hostname + "-" + std::to_string(getpid());
        
        std::vector<std::string> filters = topics_;
        filters.push_back("$SYS/broker/#");
        
        mqtt::Parser parser;
        bool failed = !sendAll(fd, mqtt::connectPacket(clientId, user_, password_, KEEP_ALIVE_SEC));
        auto lastHeard = std::chrono::steady_clock::now();
        auto lastPing = lastHeard;
        
        while (running_ && !failed) {
            auto now = std::chrono::steady_clock::now();
            if (now - lastHeard > std::chrono::seconds(KEEP_ALIVE_SEC * 3 / 2)) break;   // broker went quiet
            if (now - lastPing > std::chrono::seconds(KEEP_ALIVE_SEC / 2)) {
                failed = !sendAll(fd, mqtt::pingPacket());
                lastPing = now;
            }
            
            struct pollfd pfd{fd, POLLIN, 0};
            if (poll(&pfd, 1, 500) <= 0) continue;
            
            uint8_t buffer[4096];
            ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            lastHeard = std::chrono::steady_clock::now();
            
            parser.feed(buffer, static_cast<size_t>(n), [&](const mqtt::Packet& p) {
                if (p.type == mqtt::CONNACK) {
                    if (p.returnCode != 0) {
                        failed = true;           // refused: wrong credentials or not allowed
                    } else {
                        setState(MqttData::State::Online);
                        failed = !sendAll(fd, mqtt::subscribePacket(1, filters));
                    }
                } else if (p.type == mqtt::PUBLISH) {
                    handlePublish(p);
                }
            });
        }
        close(fd);
    }
    
    void run() {
        setState(MqttData::State::Connecting);
        while (running_) {
            session();
            if (!running_) break;
            setState(MqttData::State::Offline);
            // Try again in five seconds, staying responsive to stop()
            for (int i = 0; i < 50 && running_; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    
    std::string host_;
    int port_;
    std::string user_, password_;
    std::vector<std::string> topics_;
    
    mutable std::mutex mutex_;
    MqttData data_;
    std::set<std::string> seenTopics_;
    std::atomic<bool> running_{false};
    bool frozen_ = false;
    std::thread thread_;
};
