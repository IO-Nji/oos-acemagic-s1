#pragma once
#include <cstddef>
#include <cstdint>

// Wire protocol of the Acemagic S1 front panel (Holtek HT32, USB 04d9:fd01).
// Every command is one 4104-byte transfer: an 8-byte header and 4096 payload bytes.
// See docs/PROTOCOL.md for timing behaviour.
namespace protocol {
    constexpr uint16_t VENDOR_ID = 0x04D9;
    constexpr uint16_t PRODUCT_ID = 0xFD01;
    constexpr int INTERFACE = 1;
    constexpr uint8_t ENDPOINT_OUT = 0x02;
    
    constexpr size_t HEADER_SIZE = 8;
    constexpr size_t PAYLOAD_SIZE = 4096;
    constexpr size_t COMMAND_SIZE = HEADER_SIZE + PAYLOAD_SIZE;
    
    constexpr int DISPLAY_WIDTH = 320;
    constexpr int DISPLAY_HEIGHT = 170;
    
    constexpr uint8_t SIGNATURE = 0x55;
    
    constexpr uint8_t CMD_SETTINGS = 0xA1;
    constexpr uint8_t SETTINGS_ORIENTATION = 0xF1;
    constexpr uint8_t SETTINGS_HEARTBEAT = 0xF2;
    constexpr uint8_t SETTINGS_SET_TIME = 0xF3;
    constexpr uint8_t ORIENTATION_LANDSCAPE = 0x01;
    
    // Draw a rectangle of up to 4096 bytes of pixels. The only drawing command used:
    // the full-screen redraw (0xA3) leaves the firmware unresponsive for seconds.
    constexpr uint8_t CMD_UPDATE = 0xA2;
}
