#include "hw/panel.hpp"
#include "core/dirty_tiles.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace {
    constexpr unsigned SEND_TIMEOUT_MS = 1500;
}

Panel::Panel()
    : device_(protocol::VENDOR_ID, protocol::PRODUCT_ID, protocol::INTERFACE, protocol::ENDPOINT_OUT) {}

bool Panel::open() {
    if (!device_.open()) return false;
    
    std::time_t t = std::time(nullptr);
    std::tm now{};
    localtime_r(&t, &now);
    
    sendSettings(protocol::SETTINGS_ORIENTATION, protocol::ORIENTATION_LANDSCAPE, 0, 0);
    setTime(now);
    sendHeartbeat(now);
    return !disconnected();
}

void Panel::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    device_.close();
}

bool Panel::sendCommand() {
    switch (device_.send(command_.data(), command_.size(), SEND_TIMEOUT_MS)) {
        case UsbDevice::Result::Ok:
            return true;
        case UsbDevice::Result::Disconnected:
            disconnected_ = true;
            return false;
        default:
            return false;
    }
}

bool Panel::sendSettings(uint8_t subCommand, uint8_t a, uint8_t b, uint8_t c) {
    std::lock_guard<std::mutex> lock(mutex_);
    command_.fill(0);
    command_[0] = protocol::SIGNATURE;
    command_[1] = protocol::CMD_SETTINGS;
    command_[2] = subCommand;
    command_[3] = a;
    command_[4] = b;
    command_[5] = c;
    return sendCommand();
}

bool Panel::sendHeartbeat(const std::tm& now) {
    bool ok = sendSettings(protocol::SETTINGS_HEARTBEAT, now.tm_hour, now.tm_min, now.tm_sec);
    // After a missed keepalive the firmware may have put up its own notice
    if (!ok) repaintAll_ = true;
    return ok;
}

bool Panel::setTime(const std::tm& now) {
    return sendSettings(protocol::SETTINGS_SET_TIME, now.tm_hour, now.tm_min, now.tm_sec);
}

bool Panel::sendTile(const Framebuffer& fb, int x, int y, int w, int h) {
    std::lock_guard<std::mutex> lock(mutex_);
    command_.fill(0);
    command_[0] = protocol::SIGNATURE;
    command_[1] = protocol::CMD_UPDATE;
    command_[2] = x & 0xFF;
    command_[3] = (x >> 8) & 0xFF;
    command_[4] = y & 0xFF;
    command_[5] = (y >> 8) & 0xFF;
    command_[6] = static_cast<uint8_t>(w);
    command_[7] = static_cast<uint8_t>(h);
    
    uint8_t* dst = command_.data() + protocol::HEADER_SIZE;
    for (int row = 0; row < h; row++) {
        std::memcpy(dst, fb.pixels() + (y + row) * fb.deviceWidth() + x, w * sizeof(uint16_t));
        dst += w * sizeof(uint16_t);
    }
    return sendCommand();
}

Panel::PresentResult Panel::present(const Framebuffer& fb) {
    PresentResult result;
    const size_t pixelCount = static_cast<size_t>(fb.deviceWidth()) * fb.deviceHeight();
    
    if (repaintAll_.exchange(false)) shownValid_ = false;
    if (shown_.size() != pixelCount) {
        shown_.assign(pixelCount, 0);
        shownValid_ = false;
    }
    
    auto tiles = dirtyTiles(shownValid_ ? shown_.data() : nullptr, fb.pixels(), fb.deviceWidth(), fb.deviceHeight());
    
    bool allDelivered = true;
    for (const Tile& t : tiles) {
        if (disconnected()) break;
        if (sendTile(fb, t.x, t.y, t.w, t.h)) {
            result.sent++;
            for (int row = 0; row < t.h; row++) {
                size_t offset = static_cast<size_t>(t.y + row) * fb.deviceWidth() + t.x;
                std::copy_n(fb.pixels() + offset, t.w, shown_.begin() + offset);
            }
        } else {
            result.failed++;
            allDelivered = false;
        }
    }
    
    // While the picture is unknown, a failed tile must not look "unchanged" next time
    if (allDelivered && !disconnected()) {
        shownValid_ = true;
    } else if (!shownValid_) {
        repaintAll_ = true;
    }
    return result;
}
