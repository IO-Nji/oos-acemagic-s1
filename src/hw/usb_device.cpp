#include "hw/usb_device.hpp"
#include <libusb-1.0/libusb.h>
#include <chrono>
#include <iostream>
#include <thread>

UsbDevice::UsbDevice(uint16_t vendorId, uint16_t productId, int interfaceNumber, uint8_t endpoint)
    : vendorId_(vendorId), productId_(productId), interface_(interfaceNumber), endpoint_(endpoint) {}

UsbDevice::~UsbDevice() {
    close();
}

bool UsbDevice::open() {
    int ret = libusb_init(&context_);
    if (ret < 0) {
        std::cerr << "libusb_init failed: " << libusb_strerror(static_cast<libusb_error>(ret)) << std::endl;
        context_ = nullptr;
        return false;
    }
    
    handle_ = libusb_open_device_with_vid_pid(context_, vendorId_, productId_);
    if (!handle_) {
        std::cerr << "USB device " << std::hex << vendorId_ << ":" << productId_ << std::dec
                  << " not found or not accessible" << std::endl;
        close();
        return false;
    }
    
    // libusb detaches usbhid on claim and reattaches it on release
    libusb_set_auto_detach_kernel_driver(handle_, 1);
    
    // Another process may hold the interface for a moment after a restart
    const int attempts = 5;
    for (int i = 0; i < attempts; i++) {
        ret = libusb_claim_interface(handle_, interface_);
        if (ret != LIBUSB_ERROR_BUSY) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (ret < 0) {
        std::cerr << "Cannot claim USB interface " << interface_ << ": "
                  << libusb_strerror(static_cast<libusb_error>(ret))
                  << (ret == LIBUSB_ERROR_BUSY ? " (is another panel program running?)" : "")
                  << std::endl;
        libusb_close(handle_);
        handle_ = nullptr;
        close();
        return false;
    }
    return true;
}

void UsbDevice::close() {
    if (handle_) {
        libusb_release_interface(handle_, interface_);
        libusb_close(handle_);
        handle_ = nullptr;
    }
    if (context_) {
        libusb_exit(context_);
        context_ = nullptr;
    }
}

UsbDevice::Result UsbDevice::send(const uint8_t* data, size_t len, unsigned timeoutMs) {
    if (!handle_) return Result::Disconnected;
    
    int transferred = 0;
    int ret = libusb_interrupt_transfer(handle_, endpoint_, const_cast<unsigned char*>(data),
                                        static_cast<int>(len), &transferred, timeoutMs);
    switch (ret) {
        case 0:                      return Result::Ok;
        case LIBUSB_ERROR_TIMEOUT:   return Result::Timeout;
        case LIBUSB_ERROR_NO_DEVICE: return Result::Disconnected;
        default:                     return Result::Error;
    }
}
