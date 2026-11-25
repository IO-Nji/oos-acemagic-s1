#pragma once
#include "core/framebuffer.hpp"
#include "hw/protocol.hpp"
#include "hw/usb_device.hpp"
#include <array>
#include <atomic>
#include <ctime>
#include <mutex>
#include <vector>

// The S1 front display. Keeps a copy of what the panel shows and sends only the
// parts of a new frame that differ. Commands may be issued from several threads.
class Panel {
public:
    Panel();
    
    // Open the device, set landscape orientation and the firmware clock
    bool open();
    void close();
    
    // Keepalive, expected about once a second; without it the firmware replaces the
    // picture with a disconnection notice. Also carries the time of day.
    bool sendHeartbeat(const std::tm& now);
    bool setTime(const std::tm& now);
    
    struct PresentResult {
        int sent = 0;      // tiles delivered
        int failed = 0;    // tiles that timed out; they are retried on the next call
    };
    
    // Bring the panel in line with fb. Call from one thread only.
    PresentResult present(const Framebuffer& fb);
    
    // The device has been unplugged; the process should exit and be restarted
    bool disconnected() const { return disconnected_.load(); }
    
private:
    bool sendCommand();    // sends command_; caller holds mutex_
    bool sendSettings(uint8_t subCommand, uint8_t a, uint8_t b, uint8_t c);
    bool sendTile(const Framebuffer& fb, int x, int y, int w, int h);
    
    UsbDevice device_;
    std::mutex mutex_;                                     // guards device_ and command_
    std::array<uint8_t, protocol::COMMAND_SIZE> command_{};
    
    std::vector<uint16_t> shown_;                          // what the panel displays
    bool shownValid_ = false;
    std::atomic<bool> repaintAll_{false};                  // set when the panel may have lost the picture
    std::atomic<bool> disconnected_{false};
};
