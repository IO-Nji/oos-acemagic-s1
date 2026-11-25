#pragma once
#include <cstddef>
#include <cstdint>

struct libusb_context;
struct libusb_device_handle;

// One claimed interface of a USB device with an interrupt OUT endpoint. libusb is
// used directly because the panel's HID descriptor is malformed and hidraw rejects it.
class UsbDevice {
public:
    enum class Result { Ok, Timeout, Disconnected, Error };
    
    UsbDevice(uint16_t vendorId, uint16_t productId, int interfaceNumber, uint8_t endpoint);
    ~UsbDevice();
    UsbDevice(const UsbDevice&) = delete;
    UsbDevice& operator=(const UsbDevice&) = delete;
    
    // Find the device and claim the interface, detaching the kernel driver if needed
    bool open();
    void close();
    
    Result send(const uint8_t* data, size_t len, unsigned timeoutMs);
    
private:
    uint16_t vendorId_, productId_;
    int interface_;
    uint8_t endpoint_;
    libusb_context* context_ = nullptr;
    libusb_device_handle* handle_ = nullptr;
};
