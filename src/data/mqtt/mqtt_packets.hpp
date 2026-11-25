#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The few MQTT 3.1.1 packets a read-only monitor needs: connect, subscribe at QoS 0,
// keep alive, and receive what is published.
namespace mqtt {

enum PacketType : uint8_t {
    CONNACK = 2, PUBLISH = 3, SUBACK = 9, PINGRESP = 13,
};

struct Packet {
    uint8_t type = 0;
    uint8_t returnCode = 0;     // CONNACK: 0 means accepted
    bool retained = false;      // PUBLISH: a stored message, not one sent just now
    std::string topic;          // PUBLISH
    std::string payload;        // PUBLISH, cut to Parser::MAX_PAYLOAD
};

namespace detail {
    inline void putString(std::vector<uint8_t>& out, const std::string& s) {
        out.push_back(static_cast<uint8_t>(s.size() >> 8));
        out.push_back(static_cast<uint8_t>(s.size() & 0xFF));
        out.insert(out.end(), s.begin(), s.end());
    }
    
    // Fixed header: type and flags, then the body length in 7-bit groups
    inline std::vector<uint8_t> frame(uint8_t firstByte, const std::vector<uint8_t>& body) {
        std::vector<uint8_t> out{firstByte};
        size_t n = body.size();
        do {
            uint8_t b = n % 128;
            n /= 128;
            out.push_back(n ? b | 0x80 : b);
        } while (n);
        out.insert(out.end(), body.begin(), body.end());
        return out;
    }
}

inline std::vector<uint8_t> connectPacket(const std::string& clientId, const std::string& user,
                                          const std::string& password, uint16_t keepAliveSec) {
    std::vector<uint8_t> body;
    detail::putString(body, "MQTT");
    body.push_back(4);                                   // protocol level 3.1.1
    uint8_t flags = 0x02;                                // clean session
    if (!user.empty()) flags |= 0x80;
    if (!user.empty() && !password.empty()) flags |= 0x40;
    body.push_back(flags);
    body.push_back(static_cast<uint8_t>(keepAliveSec >> 8));
    body.push_back(static_cast<uint8_t>(keepAliveSec & 0xFF));
    detail::putString(body, clientId);
    if (flags & 0x80) detail::putString(body, user);
    if (flags & 0x40) detail::putString(body, password);
    return detail::frame(0x10, body);
}

inline std::vector<uint8_t> subscribePacket(uint16_t packetId, const std::vector<std::string>& topics) {
    std::vector<uint8_t> body{static_cast<uint8_t>(packetId >> 8), static_cast<uint8_t>(packetId & 0xFF)};
    for (const auto& t : topics) {
        detail::putString(body, t);
        body.push_back(0);                               // QoS 0
    }
    return detail::frame(0x82, body);
}

inline std::vector<uint8_t> pingPacket() {
    return {0xC0, 0x00};
}

// Turns the byte stream from the broker back into packets. Bytes can arrive in any
// grouping; a packet is reported once it is complete.
class Parser {
public:
    static constexpr size_t MAX_PAYLOAD = 512;           // enough to show; the rest is skipped
    
    void feed(const uint8_t* data, size_t len, const std::function<void(const Packet&)>& onPacket) {
        for (size_t i = 0; i < len; i++) {
            uint8_t b = data[i];
            switch (state_) {
                case State::Header:
                    first_ = b;
                    remaining_ = 0;
                    shift_ = 0;
                    body_.clear();
                    state_ = State::Length;
                    break;
                case State::Length:
                    remaining_ |= static_cast<size_t>(b & 0x7F) << shift_;
                    shift_ += 7;
                    if (!(b & 0x80)) {
                        state_ = State::Body;
                        if (remaining_ == 0) finish(onPacket);
                    } else if (shift_ > 21) {
                        state_ = State::Header;          // not MQTT; resynchronise
                    }
                    break;
                case State::Body:
                    if (body_.size() < KEEP) body_.push_back(b);
                    if (--remaining_ == 0) finish(onPacket);
                    break;
            }
        }
    }
    
private:
    enum class State { Header, Length, Body };
    static constexpr size_t KEEP = 2 + 1024 + 2 + MAX_PAYLOAD;   // topic, packet id, payload
    
    void finish(const std::function<void(const Packet&)>& onPacket) {
        state_ = State::Header;
        Packet p;
        p.type = first_ >> 4;
        
        if (p.type == CONNACK && body_.size() >= 2) {
            p.returnCode = body_[1];
        } else if (p.type == PUBLISH) {
            if (body_.size() < 2) return;
            size_t topicLen = (static_cast<size_t>(body_[0]) << 8) | body_[1];
            size_t offset = 2 + topicLen;
            if (offset > body_.size()) return;
            p.topic.assign(body_.begin() + 2, body_.begin() + offset);
            if (first_ & 0x06) offset += 2;              // QoS 1 or 2 carry a packet id
            if (offset < body_.size()) {
                size_t n = std::min(body_.size() - offset, MAX_PAYLOAD);
                p.payload.assign(body_.begin() + offset, body_.begin() + offset + n);
            }
            p.retained = first_ & 0x01;
        }
        onPacket(p);
    }
    
    State state_ = State::Header;
    uint8_t first_ = 0;
    size_t remaining_ = 0;
    int shift_ = 0;
    std::vector<uint8_t> body_;
};

}  // namespace mqtt
